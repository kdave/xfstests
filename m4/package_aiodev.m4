AC_DEFUN([AC_PACKAGE_WANT_AIO],
  [ AC_CHECK_HEADERS(libaio.h, [ have_aio=true ], [ have_aio=false ])
    AC_SUBST(have_aio)
  ])
