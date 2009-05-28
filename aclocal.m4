# generated automatically by aclocal 1.9.6 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005  Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# The AC_MULTILIB macro was extracted and modified from 
# gettext-0.15's AC_LIB_PREPARE_MULTILIB macro in the lib-prefix.m4 file
# so that the correct paths can be used for 64-bit libraries.
#
dnl Copyright (C) 2001-2005 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl From Bruno Haible.

dnl AC_MULTILIB creates a variable libdirsuffix, containing
dnl the suffix of the libdir, either "" or "64".
dnl Only do this if the given enable parameter is "yes".
AC_DEFUN([AC_MULTILIB],
[
  dnl There is no formal standard regarding lib and lib64. The current
  dnl practice is that on a system supporting 32-bit and 64-bit instruction
  dnl sets or ABIs, 64-bit libraries go under $prefix/lib64 and 32-bit
  dnl libraries go under $prefix/lib. We determine the compiler's default
  dnl mode by looking at the compiler's library search path. If at least
  dnl of its elements ends in /lib64 or points to a directory whose absolute
  dnl pathname ends in /lib64, we assume a 64-bit ABI. Otherwise we use the
  dnl default, namely "lib".
  enable_lib64="$1"
  libdirsuffix=""
  searchpath=`(LC_ALL=C $CC -print-search-dirs) 2>/dev/null | sed -n -e 's,^libraries: ,,p' | sed -e 's,^=,,'`
  if test "$enable_lib64" = "yes" -a -n "$searchpath"; then
    save_IFS="${IFS= 	}"; IFS=":"
    for searchdir in $searchpath; do
      if test -d "$searchdir"; then
        case "$searchdir" in
          */lib64/ | */lib64 ) libdirsuffix=64 ;;
          *) searchdir=`cd "$searchdir" && pwd`
             case "$searchdir" in
               */lib64 ) libdirsuffix=64 ;;
             esac ;;
        esac
      fi
    done
    IFS="$save_IFS"
  fi
  AC_SUBST(libdirsuffix)
])

AC_DEFUN([AC_PACKAGE_NEED_SYS_ACL_H],
  [ AC_CHECK_HEADERS([sys/acl.h])
    if test "$ac_cv_header_sys_acl_h" != "yes"; then
        echo
        echo 'FATAL ERROR: sys/acl.h does not exist.'
        echo 'Install the access control lists (acl) development package.'
        echo 'Alternatively, run "make install-dev" from the acl source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ACL_LIBACL_H],
  [ AC_CHECK_HEADERS([acl/libacl.h])
    if test "$ac_cv_header_acl_libacl_h" != "yes"; then
        echo
        echo 'FATAL ERROR: acl/libacl.h does not exist.'
        echo 'Install the access control lists (acl) development package.'
        echo 'Alternatively, run "make install-dev" from the acl source.'
        exit 1
    fi
  ])


AC_DEFUN([AC_PACKAGE_NEED_ACLINIT_LIBACL],
  [ AC_CHECK_LIB(acl, acl_init,, [
	echo
	echo 'FATAL ERROR: could not find a valid Access Control List library.'
	echo 'Install either the libacl (rpm) or the libacl1 (deb) package.'
	echo 'Alternatively, run "make install-lib" from the acl source.'
        exit 1
    ])
    libacl="-lacl"
    test -f `pwd`/../acl/libacl/libacl.la && \
        libacl="`pwd`/../acl/libacl/libacl.la"
    test -f ${libexecdir}${libdirsuffix}/libacl.la && \
	libacl="${libexecdir}${libdirsuffix}/libacl.la"
    AC_SUBST(libacl)
  ])

AC_DEFUN([AC_PACKAGE_WANT_AIO],
  [ AC_CHECK_HEADERS(libaio.h, [ have_aio=true ], [ have_aio=false ])
    AC_SUBST(have_aio)
  ])

AC_DEFUN([AC_PACKAGE_WANT_DMAPI],
  [ AC_CHECK_HEADERS(sys/dmapi/dmapi.h, [ have_dmapi=true ], [ have_dmapi=false ])
    AC_SUBST(have_dmapi)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTR_XATTR_H],
  [ AC_CHECK_HEADERS([attr/xattr.h])
    if test "$ac_cv_header_attr_xattr_h" != "yes"; then
        echo
        echo 'FATAL ERROR: attr/xattr.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-dev" from the attr source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTR_ERROR_H],
  [ AC_CHECK_HEADERS([attr/error_context.h])
    if test "$ac_cv_header_attr_error_context_h" != "yes"; then
        echo
        echo 'FATAL ERROR: attr/error_context.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-dev" from the attr source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRIBUTES_H],
  [ have_attributes_h=false
    AC_CHECK_HEADERS([attr/attributes.h sys/attributes.h], [have_attributes_h=true], )
    if test "$have_attributes_h" = "false"; then
        echo
        echo 'FATAL ERROR: attributes.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-dev" from the attr source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_WANT_ATTRLIST_LIBATTR],
  [ AC_CHECK_LIB(attr, attr_list, [have_attr_list=true], [have_attr_list=false])
    AC_SUBST(have_attr_list)
  ])

AC_DEFUN([AC_PACKAGE_NEED_GETXATTR_LIBATTR],
  [ AC_CHECK_LIB(attr, getxattr,, [
        echo
        echo 'FATAL ERROR: could not find a valid Extended Attributes library.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-lib" from the attr source.'
        exit 1
    ])
    libattr="-lattr"
    test -f `pwd`/../attr/libattr/libattr.la && \
        libattr="`pwd`/../attr/libattr/libattr.la"
    test -f ${libexecdir}${libdirsuffix}/libattr.la && \
	libattr="${libexecdir}${libdirsuffix}/libattr.la"
    AC_SUBST(libattr)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRGET_LIBATTR],
  [ AC_CHECK_LIB(attr, attr_get,, [
        echo
        echo 'FATAL ERROR: could not find a valid Extended Attributes library.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-lib" from the attr source.'
        exit 1
    ])
    libattr="-lattr"
    test -f `pwd`/../attr/libattr/libattr.la && \
        libattr="`pwd`/../attr/libattr/libattr.la"
    test -f ${libexecdir}${libdirsuffix}/libattr.la && \
	libattr="${libexecdir}${libdirsuffix}/libattr.la"
    AC_SUBST(libattr)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRIBUTES_MACROS],
  [ AC_MSG_CHECKING([macros in attr/attributes.h])
    AC_TRY_LINK([
#include <sys/types.h>
#include <attr/attributes.h>],
    [ int x = ATTR_SECURE; ], [ echo ok ], [
        echo
	echo 'FATAL ERROR: could not find a current attributes header.'
        echo 'Upgrade the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-dev" from the attr source.'
	exit 1 ])
  ])

AC_DEFUN([AC_PACKAGE_WANT_NDBM],
  [ AC_CHECK_HEADERS(ndbm.h, [ have_db=true ], [ have_db=false ])
    libgdbm=""
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

AC_DEFUN([AC_PACKAGE_WANT_GDBM],
  [ AC_CHECK_HEADERS([gdbm/ndbm.h], [ have_db=true ], [ have_db=false ])
    if test $have_db = true; then
	libgdbm="-lgdbm"
    fi
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

#
# Generic macro, sets up all of the global packaging variables.
# The following environment variables may be set to override defaults:
#   DEBUG OPTIMIZER MALLOCLIB PLATFORM DISTRIBUTION INSTALL_USER INSTALL_GROUP
#   BUILD_VERSION
#
AC_DEFUN([AC_PACKAGE_GLOBALS],
  [ pkg_name="$1"
    AC_SUBST(pkg_name)

    . ./VERSION
    pkg_version=${PKG_MAJOR}.${PKG_MINOR}.${PKG_REVISION}
    AC_SUBST(pkg_version)
    pkg_release=$PKG_BUILD
    test -z "$BUILD_VERSION" || pkg_release="$BUILD_VERSION"
    AC_SUBST(pkg_release)

    DEBUG=${DEBUG:-'-DDEBUG'}		dnl  -DNDEBUG
    debug_build="$DEBUG"
    AC_SUBST(debug_build)

    OPTIMIZER=${OPTIMIZER:-'-g -O2'}
    opt_build="$OPTIMIZER"
    AC_SUBST(opt_build)

    MALLOCLIB=${MALLOCLIB:-''}		dnl  /usr/lib/libefence.a
    malloc_lib="$MALLOCLIB"
    AC_SUBST(malloc_lib)

    pkg_user=`id -u -n`
    test -z "$INSTALL_USER" || pkg_user="$INSTALL_USER"
    AC_SUBST(pkg_user)

    pkg_group=`id -g -n`
    test -z "$INSTALL_GROUP" || pkg_group="$INSTALL_GROUP"
    AC_SUBST(pkg_group)

    pkg_distribution=`uname -s`
    test -z "$DISTRIBUTION" || pkg_distribution="$DISTRIBUTION"
    AC_SUBST(pkg_distribution)

    pkg_platform=`uname -s | tr 'A-Z' 'a-z' | sed -e 's/irix64/irix/'`
    test -z "$PLATFORM" || pkg_platform="$PLATFORM"
    AC_SUBST(pkg_platform)
  ])

#
# Check for specified utility (env var) - if unset, fail.
# 
AC_DEFUN([AC_PACKAGE_NEED_UTILITY],
  [ if test -z "$2"; then
        echo
        echo FATAL ERROR: $3 does not seem to be installed.
        echo $1 cannot be built without a working $4 installation.
        exit 1
    fi
  ])

#
# Generic macro, sets up all of the global build variables.
# The following environment variables may be set to override defaults:
#  CC MAKE LIBTOOL TAR ZIP MAKEDEPEND AWK SED ECHO SORT
#  MSGFMT MSGMERGE RPM
#
AC_DEFUN([AC_PACKAGE_UTILITIES],
  [ AC_PROG_CC
    cc="$CC"
    AC_SUBST(cc)
    AC_PACKAGE_NEED_UTILITY($1, "$cc", cc, [C compiler])

    if test -z "$MAKE"; then
        AC_PATH_PROG(MAKE, gmake,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
    fi
    if test -z "$MAKE"; then
        AC_PATH_PROG(MAKE, make,, /usr/bin)
    fi
    make=$MAKE
    AC_SUBST(make)
    AC_PACKAGE_NEED_UTILITY($1, "$make", make, [GNU make])

    if test -z "$LIBTOOL"; then
	AC_PATH_PROG(LIBTOOL, glibtool,, /usr/bin)
    fi
    if test -z "$LIBTOOL"; then
	AC_PATH_PROG(LIBTOOL, libtool,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
    fi
    libtool=$LIBTOOL
    AC_SUBST(libtool)
    AC_PACKAGE_NEED_UTILITY($1, "$libtool", libtool, [GNU libtool])

    if test -z "$TAR"; then
        AC_PATH_PROG(TAR, tar,, /usr/freeware/bin:/bin:/usr/local/bin:/usr/bin)
    fi
    tar=$TAR
    AC_SUBST(tar)
    if test -z "$ZIP"; then
        AC_PATH_PROG(ZIP, gzip,, /bin:/usr/bin:/usr/local/bin:/usr/freeware/bin)
    fi

    zip=$ZIP
    AC_SUBST(zip)

    if test -z "$MAKEDEPEND"; then
        AC_PATH_PROG(MAKEDEPEND, makedepend, /bin/true)
    fi
    makedepend=$MAKEDEPEND
    AC_SUBST(makedepend)

    if test -z "$AWK"; then
        AC_PATH_PROG(AWK, awk,, /bin:/usr/bin)
    fi
    awk=$AWK
    AC_SUBST(awk)

    if test -z "$SED"; then
        AC_PATH_PROG(SED, sed,, /bin:/usr/bin)
    fi
    sed=$SED
    AC_SUBST(sed)

    if test -z "$ECHO"; then
        AC_PATH_PROG(ECHO, echo,, /bin:/usr/bin)
    fi
    echo=$ECHO
    AC_SUBST(echo)

    if test -z "$SORT"; then
        AC_PATH_PROG(SORT, sort,, /bin:/usr/bin)
    fi
    sort=$SORT
    AC_SUBST(sort)

    dnl check if symbolic links are supported
    AC_PROG_LN_S

    if test "$enable_gettext" = yes; then
        if test -z "$MSGFMT"; then
                AC_PATH_PROG(MSGFMT, msgfmt,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
        fi
        msgfmt=$MSGFMT
        AC_SUBST(msgfmt)
        AC_PACKAGE_NEED_UTILITY($1, "$msgfmt", msgfmt, gettext)

        if test -z "$MSGMERGE"; then
                AC_PATH_PROG(MSGMERGE, msgmerge,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
        fi
        msgmerge=$MSGMERGE
        AC_SUBST(msgmerge)
        AC_PACKAGE_NEED_UTILITY($1, "$msgmerge", msgmerge, gettext)
    fi

    if test -z "$RPM"; then
        AC_PATH_PROG(RPM, rpm,, /bin:/usr/bin:/usr/freeware/bin)
    fi
    rpm=$RPM
    AC_SUBST(rpm)

    dnl .. and what version is rpm
    rpm_version=0
    test -n "$RPM" && test -x "$RPM" && rpm_version=`$RPM --version \
                        | awk '{print $NF}' | awk -F. '{V=1; print $V}'`
    AC_SUBST(rpm_version)
    dnl At some point in rpm 4.0, rpm can no longer build rpms, and
    dnl rpmbuild is needed (rpmbuild may go way back; not sure)
    dnl So, if rpm version >= 4.0, look for rpmbuild.  Otherwise build w/ rpm
    if test $rpm_version -ge 4; then
        AC_PATH_PROG(RPMBUILD, rpmbuild)
        rpmbuild=$RPMBUILD
    else
        rpmbuild=$RPM
    fi
    AC_SUBST(rpmbuild)
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUID_H],
  [ AC_CHECK_HEADERS([uuid.h sys/uuid.h uuid/uuid.h])
    if test $ac_cv_header_uuid_h = no -a \
	    $ac_cv_header_sys_uuid_h = no -a \
	    $ac_cv_header_uuid_uuid_h = no; then
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUIDCOMPARE],
  [ AC_CHECK_FUNCS(uuid_compare)
    if test $ac_cv_func_uuid_compare = yes; then
	libuuid=""
    else
	AC_CHECK_LIB(uuid, uuid_compare,, [
	    echo
	    echo 'FATAL ERROR: could not find a valid UUID library.'
	    echo 'Install the Universally Unique Identifiers library package.'
	    exit 1])
	libuuid="-luuid"
    fi
    AC_SUBST(libuuid)
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_XFS_H],
  [ AC_CHECK_HEADERS([xfs/xfs.h])
    if test "$ac_cv_header_xfs_xfs_h" != "yes"; then
        echo
        echo 'FATAL ERROR: cannot find a valid <xfs/xfs.h> header file.'
        echo 'Install or upgrade the XFS development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_LIBXFS_H],
  [ AC_CHECK_HEADERS([xfs/libxfs.h])
    if test "$ac_cv_header_xfs_libxfs_h" != "yes"; then
        echo
        echo 'FATAL ERROR: cannot find a valid <xfs/libxfs.h> header file.'
        echo 'Install or upgrade the XFS development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_XQM_H],
  [ AC_CHECK_HEADERS([xfs/xqm.h])
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
    test -f `pwd`/../xfsprogs/libxfs/libxfs.la && \
        libxfs="`pwd`/../xfsprogs/libxfs/libxfs.la"
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
    test -f `pwd`/../xfsprogs/libhandle/libhandle.la && \
        libhdl="`pwd`/../xfsprogs/libhandle/libhandle.la"
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
    test -f `pwd`/../xfsprogs/libhandle/libhandle.la && \
        libhdl="`pwd`/../xfsprogs/libhandle/libhandle.la"
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
    AC_TRY_LINK([#include <xfs/xfs.h>], [ int x = xfsctl(0, 0, 0, 0); ],
      [ echo ok ],
      [ echo
        echo 'FATAL ERROR: cannot find required macros in the XFS headers.'
        echo 'Upgrade your XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
      ])
  ])

