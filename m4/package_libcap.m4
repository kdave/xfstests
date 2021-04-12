AC_DEFUN([AC_PACKAGE_WANT_LIBCAP],
  [ AC_CHECK_HEADERS(sys/capability.h, [ have_libcap=true ], [ have_libcap=false ])
    AC_SUBST(have_libcap)
  ])
