AC_DEFUN([AC_PACKAGE_WANT_URING],
  [ AC_CHECK_HEADERS(liburing.h, [ have_uring=true ], [ have_uring=false ])
    AC_SUBST(have_uring)
  ])
