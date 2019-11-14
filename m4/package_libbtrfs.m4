AC_DEFUN([AC_PACKAGE_WANT_LIBBTRFSUTIL],
  [ AC_CHECK_HEADERS(btrfsutil.h, [ have_libbtrfsutil=true ],
                     [ have_libbtrfsutil=false ])
    AC_SUBST(have_libbtrfsutil)
  ])
