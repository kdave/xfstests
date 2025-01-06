AC_DEFUN([AC_PACKAGE_WANT_URING],
  [ PKG_CHECK_MODULES([LIBURING], [liburing],
    [ AC_DEFINE([HAVE_LIBURING], [1], [Use liburing])
      AC_DEFINE_UNQUOTED([LIBURING_MAJOR_VERSION], [`$PKG_CONFIG --modversion liburing | cut -d. -f1`], [liburing major version])
      AC_DEFINE_UNQUOTED([LIBURING_MINOR_VERSION], [`$PKG_CONFIG --modversion liburing | cut -d. -f2`], [liburing minor version])
      have_uring=true
    ],
    [ have_uring=false ])
    AC_SUBST(have_uring)
  ])
