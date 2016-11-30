AC_DEFUN([AC_PACKAGE_NEED_XFS_XFS_H],
  [ AC_CHECK_HEADERS([xfs/xfs.h],,,[
      #define _GNU_SOURCE
      #define _FILE_OFFSET_BITS 64
    ])
    if test "$ac_cv_header_xfs_xfs_h" != "yes"; then
        echo
        echo 'FATAL ERROR: cannot find a valid <xfs/xfs.h> header file.'
        echo 'Run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_WANT_LIBXFS_H],
  [ AC_CHECK_HEADERS([xfs/libxfs.h], [ have_libxfs=true ],
    [ have_libxfs=false ], [#define _GNU_SOURCE])
    AC_SUBST(have_libxfs)
  ])

AC_DEFUN([AC_PACKAGE_WANT_XLOG_ASSIGN_LSN],
  [ AC_CHECK_DECL(xlog_assign_lsn,
      [ have_xlog_assign_lsn=true ], [ have_xlog_assign_lsn=false ], [[
#define _GNU_SOURCE
#include <xfs/libxfs.h>]])
    AC_SUBST(have_xlog_assign_lsn)
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_XQM_H],
  [ AC_CHECK_HEADERS([xfs/xqm.h],,,[
        #define _GNU_SOURCE
        #define _FILE_OFFSET_BITS 64
    ])
    if test "$ac_cv_header_xfs_xqm_h" != "yes"; then
        echo
        echo 'FATAL ERROR: cannot find a valid <xfs/xqm.h> header file.'
        echo 'Install or upgrade the XFS development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_HANDLE_H],
  [ AC_CHECK_HEADERS([xfs/handle.h])
    if test "$ac_cv_header_xfs_handle_h" != "yes"; then
        echo
        echo 'FATAL ERROR: cannot find a valid <xfs/handle.h> header file.'
        echo 'Install or upgrade the XFS development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_LIBXFSINIT_LIBXFS],
  [ AC_CHECK_LIB(xfs, libxfs_init,, [
        echo
        echo 'FATAL ERROR: could not find a valid XFS base library.'
        echo 'Install or upgrade the XFS library package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    ])
    libxfs="-lxfs"
    test -f ${libexecdir}${libdirsuffix}/libxfs.la && \
	libxfs="${libexecdir}${libdirsuffix}/libxfs.la"
    AC_SUBST(libxfs)
  ])

AC_DEFUN([AC_PACKAGE_NEED_OPEN_BY_FSHANDLE],
  [ AC_CHECK_LIB(handle, open_by_fshandle,, [
        echo
        echo 'FATAL ERROR: could not find a current XFS handle library.'
        echo 'Install or upgrade the XFS library package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    ])
    libhdl="-lhandle"
    test -f ${libexecdir}${libdirsuffix}/libhandle.la && \
	libhdl="${libexecdir}${libdirsuffix}/libhandle.la"
    AC_SUBST(libhdl)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRLIST_LIBHANDLE],
  [ AC_CHECK_LIB(handle, attr_list_by_handle,, [
        echo
        echo 'FATAL ERROR: could not find a current XFS handle library.'
        echo 'Install or upgrade the XFS library package.'
        echo 'Alternatively, run "make install-lib" from the xfsprogs source.'
        exit 1
    ])
    libhdl="-lhandle"
    test -f ${libexecdir}${libdirsuffix}/libhandle.la && \
	libhdl="${libexecdir}${libdirsuffix}/libhandle.la"
    AC_SUBST(libhdl)
  ])

AC_DEFUN([AC_PACKAGE_NEED_IRIX_LIBHANDLE],
  [ 
    AC_MSG_CHECKING([libhandle.a for IRIX])
    libhdl="`pwd`/../irix/libhandle/libhandle.a"
    if ! test -f $libhdl; then
	echo 'no'
        echo 'FATAL ERROR: could not find IRIX XFS handle library.'
        exit 1
    fi
    echo 'yes'
    AC_SUBST(libhdl)
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFSCTL_MACRO],
  [ AC_MSG_CHECKING([xfsctl from xfs/xfs.h])
    AC_TRY_LINK([
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <xfs/xfs.h> ],
      [ int x = xfsctl(0, 0, 0, 0); ],
      [ echo ok ],
      [ echo
        echo 'FATAL ERROR: cannot find required macros in the XFS headers.'
        echo 'Upgrade your XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
      ])
  ])
