/*
 * Cryptographic API.
 *
 * LZ4KDR compression algorithm.
 *
 * LZ4KDR is a speed-tuned derivative of Huawei's LZ4KD; it keeps
 * LZ4KD's on-wire format (the compressed bitstream is byte-for-byte
 * compatible with the original decoder) and is renamed from lz4kd to
 * lz4kdr so it is not mistaken for an unmodified import of Huawei's
 * original code -- all credit for the underlying algorithm and format
 * belongs to them. See include/linux/lz4kdr.h for the full note.
 *
 * This is the crypto API wrapper that lets zram use lz4kdr. It is
 * modelled on crypto/lz4.c: 4.9's zram goes through the crypto API
 * (crypto_comp_compress()/crypto_comp_decompress()), it has no
 * zcomp_ops/backend abstraction, so registering a compress-type
 * crypto_alg named "lz4kdr" plus adding "lz4kdr" to zcomp.c's
 * backends[] string array is the whole registration story.
 *
 * Copyright (c) 2013 Chanho Min <chanho.min@lge.com>  (crypto/lz4.c,
 * this file's structural template)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/lz4kdr.h>

struct lz4kdr_ctx {
	void *state;
};

static int lz4kdr_init(struct crypto_tfm *tfm)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);
	unsigned state_bytes = lz4kdr_encode_state_bytes_min();

	/*
	 * lz4kdr requires the caller to zero the `state' scratch buffer
	 * (the encoder's hash table) exactly once before its first use.
	 * Unlike upstream LZ4KD, lz4kdr does NOT re-zero it on every
	 * compress call -- see the "change 1" note in lz4kdr_encode().
	 * vmalloc() makes no zeroing guarantee, so memset it here. This
	 * is mandatory for correctness, not an optimization.
	 */
	ctx->state = vmalloc(state_bytes);
	if (!ctx->state)
		return -ENOMEM;

	memset(ctx->state, 0, state_bytes);
	return 0;
}

static void lz4kdr_exit(struct crypto_tfm *tfm)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);

	vfree(ctx->state);
}

static int lz4kdr_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
				  unsigned int slen, u8 *dst,
				  unsigned int *dlen)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);
	int ret;

	/*
	 * in_max  = slen   (PAGE_SIZE, zcomp_compress() always feeds one
	 *                    full page)
	 * out_max = *dlen  (PAGE_SIZE * 2 -- zcomp_compress() pre-sizes
	 *                    its dst buffer that way so compressors may
	 *                    overshoot the source length)
	 * out_limit = 0    ("use min(in_max, out_max)")
	 */
	ret = lz4kdr_encode(ctx->state, src, dst, slen, *dlen, 0);
	if (ret < 0)
		return -EINVAL;

	if (ret == 0) {
		/*
		 * LZ4KDR_STATUS_INCOMPRESSIBLE. This is NOT an error --
		 * lz4kdr gives an explicit "did not compress" verdict
		 * instead of LZ4/zstd's "comp_len came back >= the huge
		 * class" signal. Report a full-page length so that
		 * __zram_bvec_write() takes its huge/raw-store path:
		 *
		 *   if (comp_len >= huge_class_size)
		 *           comp_len = PAGE_SIZE;
		 *   ...
		 *   if (comp_len == PAGE_SIZE)
		 *           src = kmap_atomic(page);
		 *
		 * i.e. zram stores the uncompressed page verbatim as a
		 * 'huge' object. Returning -EINVAL here would fail the
		 * whole write and stall reclaim on its first
		 * incompressible page.
		 */
		*dlen = slen;
		return 0;
	}

	*dlen = ret;
	return 0;
}

static int lz4kdr_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				    unsigned int slen, u8 *dst,
				    unsigned int *dlen)
{
	int ret;

	/*
	 * in_max is the size of the *compressed* stream. lz4kdr_decode()
	 * derives its input bound as `in_end = in + in_max' and both of
	 * its exits require the input cursor to land exactly on in_end
	 * (end_of_block(): "if (in_at != in_end) return FAILED;", and
	 * decode()'s tail: "return in_at == in_end ? ... : FAILED;").
	 * So in_max must be the true stored length -- `slen', which is
	 * what zcomp_decompress() passes as the object size -- and not
	 * PAGE_SIZE. Anything larger than the real stream length makes
	 * every decompression fail.
	 *
	 * out_max = *dlen (PAGE_SIZE, set by zcomp_decompress()).
	 */
	ret = lz4kdr_decode(src, dst, slen, *dlen);
	if (ret < 0)
		return -EINVAL;

	*dlen = ret;
	return 0;
}

static struct crypto_alg alg_lz4kdr = {
	.cra_name		= "lz4kdr",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= sizeof(struct lz4kdr_ctx),
	.cra_module		= THIS_MODULE,
	.cra_list		= LIST_HEAD_INIT(alg_lz4kdr.cra_list),
	.cra_init		= lz4kdr_init,
	.cra_exit		= lz4kdr_exit,
	.cra_u			= { .compress = {
	.coa_compress		= lz4kdr_compress_crypto,
	.coa_decompress		= lz4kdr_decompress_crypto } }
};

static int __init lz4kdr_mod_init(void)
{
	return crypto_register_alg(&alg_lz4kdr);
}

static void __exit lz4kdr_mod_fini(void)
{
	crypto_unregister_alg(&alg_lz4kdr);
}

module_init(lz4kdr_mod_init);
module_exit(lz4kdr_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ4KDR Compression Algorithm");
MODULE_ALIAS_CRYPTO("lz4kdr");
