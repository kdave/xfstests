AC_INIT([xfstests], [1.1.1])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT
AC_CONFIG_SRCDIR([src/xfsctl.c])
AC_PACKAGE_GLOBALS(xfstests)
AC_PACKAGE_UTILITIES(xfstests)

AC_HEADER_STDC
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
    ])

AC_CHECK_HEADERS([xfs/xfs_log_format.h],,,[
#define _GNU_SOURCE
#include <xfs/libxfs.h>])

AC_PACKAGE_NEED_UUIDCOMPARE

case $pkg_platform
in
	irix)
		AC_PACKAGE_NEED_SYS_ACL_H
		AC_PACKAGE_NEED_ATTRIBUTES_H
		AC_PACKAGE_WANT_NDBM
		AC_PACKAGE_NEED_IRIX_LIBHANDLE
		have_attr_list=true
		AC_SUBST(have_attr_list)
		;;
	*)
		AC_MULTILIB($enable_lib64)
		AC_PACKAGE_NEED_XFS_XFS_H
		AC_PACKAGE_WANT_LIBXFS_H
		AC_PACKAGE_WANT_XLOG_ASSIGN_LSN
		AC_PACKAGE_NEED_XFS_XQM_H
		AC_PACKAGE_NEED_XFSCTL_MACRO
		AC_PACKAGE_NEED_XFS_HANDLE_H

		AC_PACKAGE_NEED_ATTRLIST_LIBHANDLE
		AC_PACKAGE_NEED_ATTR_XATTR_H
		AC_PACKAGE_NEED_ATTRIBUTES_H
		AC_PACKAGE_WANT_ATTRLIST_LIBATTR
		AC_PACKAGE_NEED_GETXATTR_LIBATTR

		AC_PACKAGE_NEED_SYS_ACL_H
		AC_PACKAGE_NEED_ACL_LIBACL_H
		AC_PACKAGE_NEED_ACLINIT_LIBACL

		AC_PACKAGE_WANT_GDBM
		AC_PACKAGE_WANT_AIO
		AC_PACKAGE_WANT_DMAPI
		AC_PACKAGE_WANT_LINUX_FIEMAP_H
		AC_PACKAGE_WANT_FALLOCATE
		AC_PACKAGE_WANT_OPEN_BY_HANDLE_AT
		AC_PACKAGE_WANT_LINUX_PRCTL_H
		AC_PACKAGE_WANT_LINUX_FS_H
		;;
esac

AC_CHECK_FUNCS([renameat2])

AC_CONFIG_HEADER(include/config.h)
AC_CONFIG_FILES([include/builddefs])
AC_OUTPUT
