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
    test -f ${libexecdir}${libdirsuffix}/libacl.la && \
	libacl="${libexecdir}${libdirsuffix}/libacl.la"
    AC_SUBST(libacl)
  ])
