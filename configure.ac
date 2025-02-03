AC_INIT([xfstests],[1.1.2])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT
AC_CONFIG_SRCDIR([src/xfsctl.c])
AC_PACKAGE_GLOBALS(xfstests)
AC_PACKAGE_UTILITIES(xfstests)

AC_CHECK_HEADERS([	assert.h		\
			bstring.h		\
			libgen.h		\
			dirent.h		\
			errno.h			\
			malloc.h		\
			uuid.h			\
			uuid/uuid.h		\
			sys/uuid.h		\
			sys/file.h		\
			sys/fcntl.h		\
			sys/syssgi.h		\
			sys/param.h		\
			sys/stat.h		\
			sys/statvfs.h		\
			sys/time.h		\
			sys/ioctl.h		\
			sys/wait.h		\
			sys/types.h		\
			strings.h		\
			err.h			\
			linux/falloc.h		\
			sys/fs/xfs_fsops.h	\
			sys/fs/xfs_itable.h	\
			xfs/platform_defs.h	\
			btrfs/ioctl.h		\
			cifs/ioctl.h		\
			sys/mman.h		\
			linux/ext4.h		\
])

AC_CHECK_HEADERS([xfs/xfs_log_format.h],,,[
#define _GNU_SOURCE
#include <xfs/libxfs.h>])

AC_PACKAGE_NEED_UUIDCOMPARE

AC_MULTILIB($enable_lib64)
AC_PACKAGE_NEED_XFS_XFS_H
AC_PACKAGE_WANT_LIBXFS_H
AC_PACKAGE_WANT_XLOG_ASSIGN_LSN
AC_PACKAGE_NEED_XFS_XQM_H
AC_PACKAGE_NEED_XFSCTL_MACRO
AC_PACKAGE_NEED_XFS_HANDLE_H
AC_PACKAGE_NEED_ATTRLIST_LIBHANDLE

AC_PACKAGE_NEED_SYS_ACL_H
AC_PACKAGE_NEED_ACL_LIBACL_H
AC_PACKAGE_NEED_ACLINIT_LIBACL

AC_PACKAGE_WANT_GDBM
AC_PACKAGE_WANT_AIO
AC_PACKAGE_WANT_URING
AC_PACKAGE_WANT_LIBCAP
AC_PACKAGE_WANT_LINUX_FIEMAP_H
AC_PACKAGE_WANT_FALLOCATE
AC_PACKAGE_WANT_OPEN_BY_HANDLE_AT
AC_PACKAGE_WANT_LINUX_FS_H
AC_PACKAGE_WANT_LIBBTRFSUTIL

AC_HAVE_COPY_FILE_RANGE
AC_HAVE_SEEK_DATA
AC_HAVE_BMV_OF_SHARED
AC_HAVE_NFTW
AC_HAVE_RLIMIT_NOFILE
AC_NEED_INTERNAL_XFS_IOC_EXCHANGE_RANGE
AC_HAVE_FICLONE
AC_HAVE_TRIVIAL_AUTO_VAR_INIT

AC_CHECK_FUNCS([renameat2])
AC_CHECK_FUNCS([reallocarray])
AC_CHECK_TYPES([struct mount_attr], [], [], [[#include <linux/mount.h>]])
AC_CHECK_TYPES([struct btrfs_qgroup_limit], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_qgroup_inherit], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_ioctl_vol_args], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_ioctl_vol_args_v2], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_ioctl_ino_lookup_args], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_ioctl_ino_lookup_user_args], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_TYPES([struct btrfs_ioctl_get_subvol_rootref_args], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_HEADERS([linux/btrfs.h linux/btrfs_tree.h])
AC_CHECK_MEMBERS([struct btrfs_ioctl_vol_args_v2.subvolid], [], [], [[
#include <stddef.h>
#include <linux/btrfs.h>
]])
AC_CHECK_DECLS([BTRFS_IOC_SNAP_DESTROY_V2],,,[#include <linux/btrfs.h>])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([include/builddefs])
AC_OUTPUT
