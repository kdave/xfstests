AC_DEFUN([AC_PACKAGE_WANT_LIBGDBM],
  [ AC_CHECK_HEADER([gdbm/ndbm.h], [have_db=true ], [ have_db=false ])
    if test $have_db = true -a -f /usr/lib/libgdbm.a; then
	libgdbm="/usr/lib/libgdbm.a"
    fi
    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])
