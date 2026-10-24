# Makefile for NoMount
#
# Ported for this tree (Linux 4.9 / msm8953):
#   * the upstream git-walk that derived NOMOUNT_COMMIT_COUNT is dropped: it is
#     dead code here, because fs/nomount is a plain directory in the kernel repo
#     (the upstream checkout lives in the drivers/nomount submodule);
#   * -fno-stack-protector is dropped: the file builds fine under the kernel's
#     own -fstack-protector-strong, and silently disabling it in a file that
#     hooks the VFS would be a needless hardening regression.

obj-$(CONFIG_NOMOUNT) += nomount.o
ccflags-y += -std=gnu11 -Wno-declaration-after-statement -O3

ifeq ($(shell grep -qE '\(\*[[:space:]]*__get\)' $(srctree)/include/linux/xattr.h 2>/dev/null && echo 1 || echo 0),1)
ccflags-y += -DNM_HAS_XATTR__GET
endif
