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

AC_DEFUN([AC_PACKAGE_NEED_XFSCTL_MACRO],
  [ AC_MSG_CHECKING([xfsctl from xfs/xfs.h])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <xfs/xfs.h> ]], [[ int x = xfsctl(0, 0, 0, 0); ]])],[ echo ok ],[ echo
        echo 'FATAL ERROR: cannot find required macros in the XFS headers.'
        echo 'Upgrade your XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
      ])
  ])

# Check if we have BMV_OF_SHARED from the GETBMAPX ioctl
AC_DEFUN([AC_HAVE_BMV_OF_SHARED],
  [ AC_MSG_CHECKING([for BMV_OF_SHARED])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <xfs/xfs.h>
    ]], [[
         struct getbmapx obj;
         ioctl(-1, XFS_IOC_GETBMAPX, &obj);
         obj.bmv_oflags |= BMV_OF_SHARED;
    ]])],[have_bmv_of_shared=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_bmv_of_shared)
  ])

# Check if we need to override the system XFS_IOC_EXCHANGE_RANGE
AC_DEFUN([AC_NEED_INTERNAL_XFS_IOC_EXCHANGE_RANGE],
  [ AC_MSG_CHECKING([for new enough XFS_IOC_EXCHANGE_RANGE])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <xfs/xfs.h>
    ]], [[
         struct xfs_exchange_range obj;
         ioctl(-1, XFS_IOC_EXCHANGE_RANGE, &obj);
    ]])],[AC_MSG_RESULT(yes)],
         [need_internal_xfs_ioc_exchange_range=yes
          AC_MSG_RESULT(no)])
    AC_SUBST(need_internal_xfs_ioc_exchange_range)
  ])
