AC_DEFUN([AC_PACKAGE_WANT_NDBM],
  [ AC_CHECK_HEADERS(ndbm.h, [ have_db=true ], [ have_db=false ])
    found=false
    libgdbm=""

    if test $have_db = true; then
      AC_CHECK_LIB(ndbm,dbm_open,found=true,found=false)
      AC_CHECK_LIB(ndbm,dbm_fetch,,found=false)
      AC_CHECK_LIB(ndbm,dbm_store,,found=false)
      AC_CHECK_LIB(ndbm,dbm_close,,found=false)

      if test $found = true; then
        libgdbm="$ndbm"
      fi
    fi

    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

AC_DEFUN([AC_PACKAGE_WANT_GDBM],
  [ AC_CHECK_HEADERS([gdbm/ndbm.h, gdbm-ndbm.h], [ have_db=true ], [ have_db=false ])
    found=false
    libgdbm=""

    if test $have_db = true; then
      AC_CHECK_LIB(gdbm,dbm_open,found=true,found=false)
      AC_CHECK_LIB(gdbm,dbm_fetch,,found=false)
      AC_CHECK_LIB(gdbm,dbm_store,,found=false)
      AC_CHECK_LIB(gdbm,dbm_close,,found=false)

      if test $found = true; then
        libgdbm="${libgdbm} -lgdbm"
      fi

      found="no"
      AC_CHECK_LIB(gdbm_compat,dbm_open,found=true,found=false)
      AC_CHECK_LIB(gdbm_compat,dbm_fetch,,found=false)
      AC_CHECK_LIB(gdbm_compat,dbm_store,,found=false)
      AC_CHECK_LIB(gdbm_compat,dbm_close,,found="no")

      if test $found = true ; then
        libgdbm="${libgdbm} -lgdbm_compat"
      fi
    fi

    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

