dnl aclocal.m4 generated automatically by aclocal 1.4-p6

dnl Copyright (C) 1994, 1995-8, 1999, 2001 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

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

    OPTIMIZER=${OPTIMIZER:-'-g'}	dnl  -O2
    opt_build="$OPTIMIZER"
    AC_SUBST(opt_build)

    MALLOCLIB=${MALLOCLIB:-''}		dnl  /usr/lib/libefence.a
    malloc_lib="$MALLOCLIB"
    AC_SUBST(malloc_lib)

    PKG_USER=${INSTALL_USER:-'root'}
    pkg_user="$PKG_USER"
    AC_SUBST(pkg_user)

    PKG_GROUP=${INSTALL_GROUP:-'root'}
    pkg_group="$PKG_GROUP"
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
  [ if test -z "$CC"; then
        AC_PROG_CC
    fi
    cc="$CC"
    AC_SUBST(cc)
    AC_PACKAGE_NEED_UTILITY($1, "$cc", cc, [C compiler])

    if test -z "$MAKE"; then
        AC_PATH_PROG(MAKE, make, /usr/bin/make)
    fi
    make=$MAKE
    AC_SUBST(make)
    AC_PACKAGE_NEED_UTILITY($1, "$make", make, [GNU make])

    if test -z "$LIBTOOL"; then
	AC_PATH_PROG(LIBTOOL, libtool,,/usr/bin:/usr/local/bin)
    fi
    libtool=$LIBTOOL
    AC_SUBST(libtool)
    AC_PACKAGE_NEED_UTILITY($1, "$libtool", libtool, [GNU libtool])

    if test -z "$TAR"; then
        AC_PATH_PROG(TAR, tar)
    fi
    tar=$TAR
    AC_SUBST(tar)
    if test -z "$ZIP"; then
        AC_PATH_PROG(ZIP, gzip, /bin/gzip)
    fi
    zip=$ZIP
    AC_SUBST(zip)
    if test -z "$MAKEDEPEND"; then
        AC_PATH_PROG(MAKEDEPEND, makedepend, /bin/true)
    fi
    makedepend=$MAKEDEPEND
    AC_SUBST(makedepend)
    if test -z "$AWK"; then
        AC_PATH_PROG(AWK, awk, /bin/awk)
    fi
    awk=$AWK
    AC_SUBST(awk)
    if test -z "$SED"; then
        AC_PATH_PROG(SED, sed, /bin/sed)
    fi
    sed=$SED
    AC_SUBST(sed)
    if test -z "$ECHO"; then
        AC_PATH_PROG(ECHO, echo, /bin/echo)
    fi
    echo=$ECHO
    AC_SUBST(echo)
    if test -z "$SORT"; then
        AC_PATH_PROG(SORT, sort, /bin/sort)
    fi
    sort=$SORT
    AC_SUBST(sort)

    dnl check if symbolic links are supported
    AC_PROG_LN_S

    if test "$enable_gettext" = yes; then
        if test -z "$MSGFMT"; then
                AC_CHECK_PROG(MSGFMT, msgfmt, /usr/bin/msgfmt)
        fi
        msgfmt=$MSGFMT
        AC_SUBST(msgfmt)
        AC_PACKAGE_NEED_UTILITY($1, "$msgfmt", msgfmt, gettext)
        if test -z "$MSGMERGE"; then
                AC_CHECK_PROG(MSGMERGE, msgmerge, /usr/bin/msgmerge)
        fi
        msgmerge=$MSGMERGE
        AC_SUBST(msgmerge)
        AC_PACKAGE_NEED_UTILITY($1, "$msgmerge", msgmerge, gettext)
    fi

    if test -z "$RPM"; then
        AC_PATH_PROG(RPM, rpm, /bin/rpm)
    fi
    rpm=$RPM
    AC_SUBST(rpm)
    dnl .. and what version is rpm
    rpm_version=0
    test -x $RPM && rpm_version=`$RPM --version \
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
  [ AC_CHECK_HEADERS(uuid.h)
    if test $ac_cv_header_uuid_h = no; then
	AC_CHECK_HEADERS(uuid/uuid.h,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1])
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUIDCOMPARE],
  [ AC_CHECK_FUNCS(uuid_compare)
    if test $ac_cv_func_uuid_compare = no; then
	AC_CHECK_LIB(uuid, uuid_compare, [libuuid=/usr/lib/libuuid.a], [
	echo
	echo 'FATAL ERROR: could not find a valid UUID library.'
	echo 'Install the Universally Unique Identifiers library package.'
	exit 1])
    fi
    AC_SUBST(libuuid)
  ])

AC_DEFUN([AC_PACKAGE_CHECK_LIBUUID],
  [ test $pkg_platform = freebsd && libuuid=""
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_LIBXFS_H],
  [ AC_CHECK_HEADERS([xfs/libxfs.h])
    if test "$ac_cv_header_xfs_libxfs_h" != "yes"; then
        echo
        echo 'FATAL ERROR: XFS header <xfs/libxfs.h> does not exist.'
        echo 'Install the XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFS_HANDLE_H],
  [ AC_CHECK_HEADERS([xfs/handle.h])
    if test "$ac_cv_header_xfs_handle_h" != "yes"; then
        echo
        echo 'FATAL ERROR: XFS header <xfs/handle.h> does not exist.'
        echo 'Install the XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_LIBXFSINIT_LIBXFS],
  [ AC_CHECK_LIB(xfs, libxfs_init,, [
        echo
        echo 'FATAL ERROR: could not find a valid XFS base library.'
        echo 'Install the XFS programs (xfsprogs) library package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
    ])
    libxfs="-lxfs"
    test -f `pwd`/../xfsprogs/libxfs/libxfs.la && \
        libxfs="`pwd`/../xfsprogs/libxfs/libxfs.la"
    test -f /usr/lib/libxfs.la && libxfs="/usr/lib/libxfs.la"
    AC_SUBST(libxfs)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRLIST_LIBHANDLE],
  [ AC_CHECK_LIB(handle, attr_list_by_handle,, [
        echo
        echo 'FATAL ERROR: could not find a current XFS handle library.'
        echo 'Install the XFS programs (xfsprogs) library package.'
        echo 'Alternatively, run "make install-lib" from the xfsprogs source.'
        exit 1
    ])
    libhdl="-lhandle"
    test -f `pwd`/../xfsprogs/libhandle/libhandle.la && \
        libhdl="`pwd`/../xfsprogs/libhandle/libhandle.la"
    test -f /usr/lib/libhandle.la && libhdl="/usr/lib/libhandle.la"
    AC_SUBST(libhdl)
  ])

AC_DEFUN([AC_PACKAGE_NEED_XFSCTL_MACRO],
  [ AC_MSG_CHECKING([xfsctl from xfs/libxfs.h])
    AC_TRY_LINK([#include <xfs/libxfs.h>], [ int x = xfsctl(0, 0, 0, 0); ],
      [ echo ok ],
      [ echo
        echo 'FATAL ERROR: cannot find required macros in the XFS headers.'
        echo 'Upgrade your XFS programs (xfsprogs) development package.'
        echo 'Alternatively, run "make install-dev" from the xfsprogs source.'
        exit 1
      ])
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTR_XATTR_H],
  [ AC_CHECK_HEADERS([attr/xattr.h])
    if test "$ac_cv_header_attr_xattr_h" != "yes"; then
        echo
        echo 'FATAL ERROR: attr/xattr.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-lib" from the attr source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTR_ERROR_H],
  [ AC_CHECK_HEADERS([attr/error_context.h])
    if test "$ac_cv_header_attr_error_context_h" != "yes"; then
        echo
        echo 'FATAL ERROR: attr/error_context.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-lib" from the attr source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTR_ATTRIBUTES_H],
  [ AC_CHECK_HEADERS([attr/attributes.h])
    if test "$ac_cv_header_attr_attributes_h" != "yes"; then
        echo
        echo 'FATAL ERROR: attr/attributes.h does not exist.'
        echo 'Install the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-lib" from the attr source.'
        exit 1
    fi
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
    test -f /usr/lib/libattr.la && libattr="/usr/lib/libattr.la"
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
    test -f /usr/lib/libattr.la && libattr="/usr/lib/libattr.la"
    AC_SUBST(libattr)
  ])

AC_DEFUN([AC_PACKAGE_NEED_ATTRIBUTES_MACROS],
  [ AC_MSG_CHECKING([macros in attr/attributes.h])
    AC_TRY_LINK([
#include <sys/types.h>
#include <attr/attributes.h>],
    [ int x = ATTR_SHIFT; ], [
        echo
        echo 'FATAL ERROR: incorrect macros exist in attributes.h header file.'
        echo 'Upgrade the extended attributes (attr) development package.'
        echo 'Alternatively, run "make install-dev" from the attr source.'
        exit 1 ],
    [ echo ok ])
  ])

AC_DEFUN([AC_PACKAGE_NEED_SYS_ACL_H],
  [ AC_CHECK_HEADERS([sys/acl.h])
    if test "$ac_cv_header_sys_acl_h" != "yes"; then
        echo
        echo 'FATAL ERROR: sys/acl.h does not exist.'
        echo 'Install the access control lists (acl) development package.'
        echo 'Alternatively, run "make install-lib" from the acl source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_ACL_LIBACL_H],
  [ AC_CHECK_HEADERS([acl/libacl.h])
    if test "$ac_cv_header_acl_libacl_h" != "yes"; then
        echo
        echo 'FATAL ERROR: acl/libacl.h does not exist.'
        echo 'Install the access control lists (acl) development package.'
        echo 'Alternatively, run "make install-lib" from the acl source.'
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
    test -f /usr/lib/libacl.la && libacl="/usr/lib/libacl.la"
    AC_SUBST(libacl)
  ])

AC_DEFUN([AC_PACKAGE_WANT_LIBGDBM],
  [ AC_CHECK_HEADER([gdbm/ndbm.h], [have_db=true ], [ have_db=false ])
    if test $have_db = true -a -f /usr/lib/libgdbm.a; then
	libgdbm="/usr/lib/libgdbm.a"
    fi
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

