/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal net/gso.h shim for kernel 4.9 (WireGuard stable backport).
 * 4.9 provides __skb_gso_segment(skb, features, tx_path); newer kernels
 * wrap it as skb_gso_segment() with an extra max_segs arg.
 */
#ifndef _NET_GSO_H
#define _NET_GSO_H

#include <linux/skbuff.h>

struct sk_buff *__skb_gso_segment(struct sk_buff *skb,
				  netdev_features_t features, bool tx_path);

static inline struct sk_buff *skb_gso_segment(struct sk_buff *skb,
					      netdev_features_t features)
{
	return __skb_gso_segment(skb, features, true);
}

#endif /* _NET_GSO_H */
