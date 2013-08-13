AC_DEFUN([AC_PACKAGE_WANT_SSL],
  [ AC_CHECK_HEADERS(openssl/md5.h, [ have_ssl=true ], [ have_ssl=false ])
    AC_SUBST(have_ssl)
  ])
