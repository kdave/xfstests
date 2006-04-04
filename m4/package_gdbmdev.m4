AC_DEFUN([AC_PACKAGE_WANT_NDBM],
  [ AC_CHECK_HEADERS(ndbm.h, [ have_db=true ], [ have_db=false ])
    libgdbm=""
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

AC_DEFUN([AC_PACKAGE_WANT_GDBM],
  [ AC_CHECK_HEADERS([gdbm/ndbm.h], [ have_db=true ], [ have_db=false ])
    if test $have_db = true -a -f ${libexecdir}${libdirsuffix}/libgdbm.a; then
	libgdbm="${libexecdir}${libdirsuffix}/libgdbm.a"
    fi
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])
