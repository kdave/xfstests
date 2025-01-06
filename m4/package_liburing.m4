AC_DEFUN([AC_PACKAGE_WANT_URING],
  [ PKG_CHECK_MODULES([LIBURING], [liburing],
    [ AC_DEFINE([HAVE_LIBURING], [1], [Use liburing])
      have_uring=true
    ],
    [ have_uring=false ])
    AC_SUBST(have_uring)
  ])
