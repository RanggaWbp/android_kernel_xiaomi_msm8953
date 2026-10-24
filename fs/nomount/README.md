# NoMount - 4.9 port notes (msm8953 / tissol)

Upstream: <https://github.com/maxsteeel/nomount> @ `bf1847f98fed382369c43bfd652cd93774b9b8e3` (v2.2.0, branch `master`),
vendored as the git submodule **`drivers/nomount`** and compiled from **`fs/nomount/`**.

## Layout

| Path | What it is |
| --- | --- |
| `drivers/nomount/` | git submodule -> `https://github.com/maxsteeel/nomount`, branch `master`, pinned at v2.2.0. Upstream source of truth, kept for diffing/updates. |
| `fs/nomount/` | the code that actually builds: `nomount.c`, `nomount.h`, `Kconfig`, `Makefile`, `README.md`. |

Why a copy instead of the upstream `fs/nomount -> ../drivers/nomount/kernel/src`
symlink (what `kernel/setup.sh` does):

* this port needed real edits to `nomount.c` / `nomount.h`, and edits inside a
  submodule working tree are exactly the thing submodules are bad at;
* `kernel/README.md` "Method 3" (copy `kernel/src/*` into `fs/nomount/`) is the
  documented manual integration path and needs no symlink;
* kbuild 4.9 does not like a symlinked source directory under `O=` builds.

To pull an upstream update:

```sh
git -C drivers/nomount fetch --depth 1 origin master
git -C drivers/nomount checkout <ref>
cp -f drivers/nomount/kernel/src/* fs/nomount/
# then re-apply the three local deltas listed under "Local deltas" below
git diff fs/nomount   # review
```

## Wiring

* `fs/Kconfig`  - `source "fs/nomount/Kconfig"` before the final `endmenu`
* `fs/Makefile` - `obj-$(CONFIG_NOMOUNT) += nomount/`
* `arch/arm64/configs/tissot_defconfig` - `CONFIG_NOMOUNT=y`

There is **no** separate "Integration Patch". Upstream ships no `kernel/*.patch`:
the VFS hooks are installed at runtime by `nomount.c` itself, which swaps
`inode_operations` / `file_operations` / `super_operations` / `dentry_operations`
pointers on the target directory's inode the moment a rule is added
(`nm_hijack_dir_ops`, `nm_hijack_dentry_ops`, `nm_hijack_superblock`). So the only
tree-level integration is the Kconfig + Makefile hookup above.

## Porting to the 4.9 VFS API

`nomount.c` already carried `LINUX_VERSION_CODE` guards back to 4.11, and on 4.9
essentially every one of them resolves to the correct legacy branch. Verified
present in this tree (`include/`):

* `path_equal()` - `include/linux/path.h`
* `d_backing_inode()` - `include/linux/dcache.h`
* `IOP_NOFOLLOW`, `S_NOATIME`/`S_NOCMTIME`/`S_NOSEC`, `inode->i_opflags` - `fs.h`
* `full_name_hash(const void *salt, const char *, unsigned int)` - `stringhash.h`
  (32-bit return; all uses assign into `u32`/`unsigned int`, no change needed)
* `s_op->evict_inode`, `s_op->drop_inode`, `d_u.d_alias`, `sb->s_inodes` - `fs.h`
* `xattr_handler.{get,set}` without `__get`/idmap - `xattr.h`
  (`NM_HAS_XATTR__GET` therefore stays undefined, see the Makefile probe)
* `iterate` *and* `iterate_shared` - both filled in `nm_dir_fops`
* `getattr(struct vfsmount *, struct dentry *, struct kstat *)`,
  `vfs_getattr_nosec(path, kstat)`, `generic_fillattr(inode, kstat)` - `fs.h`
* `listxattr`, `get_link(..., struct delayed_call *)`, `setattr`, `lookup(..., flags)`,
  `d_revalidate(dentry, flags)`, `filldir_t` - all the pre-5.x signatures
* `dir_context` / `dir_emit` / `dir_emit_dots`, `QSTR_INIT`, `hash_*_rcu`,
  `get_user_pages_fast(..., FOLL_WRITE, ...)`, `vfs_llseek`, `dentry_open(path, flags, cred)`
* `SQUASHFS_MAGIC` - `include/uapi/linux/magic.h`
* `__UNIQUE_ID` - `compiler-gcc.h` / `compiler-clang.h` (needed by `rcu_guard()`)
* `smp_load_acquire` / `smp_store_release` - `asm-generic/barrier.h`; the
  `compiletime_assert_atomic_type` inside them is a *size* check
  (`__native_word`), so the pointer stores the code does (`sb->s_op`,
  `sb->s_xattr`, `inode->i_op`, `dentry->d_op`) are accepted on arm64.
* `DCACHE_DONTCACHE` - not in 4.9, but `nomount.h` already `#ifndef`s it to 0.
* `EROFS_SUPER_MAGIC_V1` - not in 4.9 (EROFS landed in 4.19), already `#ifdef`'d.

Not touched: `do_new_mount` / `do_umount` / `path_lookup`. NoMount does not hook
the mount path at all - that is the whole point of the design (it never creates a
mount), so those APIs are irrelevant here.

## Local deltas (things that differ from upstream `kernel/src/`)

1. **Boot-safety master switch** (`nomount.c`, `nomount.h`, `Kconfig`).
   The subsystem is compiled in but *disarmed* by default:
   `nomount_init()` only registers the `nomount` keyring key type; no VFS
   operation vector is replaced and every payload is rejected with `-EACCES`
   until the switch is flipped. Default value of `nm_enabled` is
   `IS_ENABLED(CONFIG_NOMOUNT_DEFAULT_ON)`, and `CONFIG_NOMOUNT_DEFAULT_ON`
   defaults to `n`. Arm it via

   ```sh
   nomount_enabled=1                                          # kernel cmdline
   echo 1 > /sys/module/nomount/parameters/nomount_enabled    # runtime
   add_key ... nomount <NM_CMD_SET_ENABLED payload>           # keyring
   ```

   `NM_CMD_SET_ENABLED` is a new command appended *after* the existing enum
   entries, so the wire protocol of the upstream `nm` tool / metamodule is
   unchanged. Without this delta a build could still not bootloop (no rules =
   no hooks), but the switch makes that guarantee explicit and gives an
   out-of-band kill switch.

   **Consequence for the metamodule:** the NoMount KernelSU metamodule and the
   `nm` binary push rules over the keyring, so they need the subsystem armed
   first. Add e.g. `echo 1 > /sys/module/nomount/parameters/nomount_enabled`
   to the module's `service.sh` (or set `CONFIG_NOMOUNT_DEFAULT_ON=y` if you
   prefer to arm at boot).

2. **`Kconfig`** - `NOMOUNT` changed from `default y` to `default n`
   (explicitly enabled in `tissot_defconfig`), plus the new
   `NOMOUNT_DEFAULT_ON` option.

3. **`Makefile`** - dropped the git-walk that derives `NOMOUNT_COMMIT_COUNT`
   (dead code here: `fs/nomount` is a plain directory in the kernel repo, the
   upstream checkout lives in the submodule) and dropped `-fno-stack-protector`.
   The file compiles cleanly under the kernel's own `-fstack-protector-strong`,
   and silently disabling it in a file that hooks the VFS would be a needless
   hardening regression. `-O3` and `-std=gnu11` are kept from upstream.

## Compile checks

`make`-extracted clang command line, executed against `O=/var/tmp/kobj`:

| object | result |
| --- | --- |
| `fs/nomount/nomount.o` | RC=0, 308944 B, no warnings |
| `fs/namespace.o` | RC=0, 436408 B (genksyms + `ld -r`) |
| `fs/namei.o` | RC=0, 528944 B (genksyms + `ld -r`) |

Note: a full `make` cannot be driven to completion on this host because
`prepare` fails in `arch/arm64/kernel/vdso32` (the host `as` has no `-EL` for
the 32-bit VDSO). That failure is pre-existing and unrelated to NoMount; it also
blocks `fs/ext4`, `fs/f2fs` and `fs/overlayfs` on this host for the same reason
(no `arm-linux-gnueabi-ld`).
