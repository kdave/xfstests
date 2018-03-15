AC_DEFUN([AC_PACKAGE_WANT_GDBM],
  [ AC_CHECK_HEADER(gdbm-ndbm.h, [ gdbm_ndbm=true; have_db=true ], [ gdbm_ndbm=false; have_db=false ])

    if test $gdbm_ndbm = true; then
        AC_DEFINE(HAVE_GDBM_NDBM_H, [1], [Define to 1 if you have the <gdbm-ndbm.h> header file.])
    else
        AS_UNSET([ac_cv_header_gdbm_ndbm_h])
        AC_CHECK_HEADER(gdbm/ndbm.h, [ gdbm_ndbm_=true; have_db=true ], [ gdbm_ndbm_=false; have_db=false ])
        if test $gdbm_ndbm_ = true; then
            AC_DEFINE(HAVE_GDBM_NDBM_H_, [1], [Define to 1 if you have the <gdbm/ndbm.h> header file.])
        else
            AC_CHECK_HEADER(gdbm.h, [ gdbm_ndbm_=true; have_db=true ], [ gdbm_ndbm_=false; have_db=false ])
            AC_CHECK_HEADER(ndbm.h, [ ndbm_=true ], [ ndbm_=false ])
                if test $gdbm_ndbm_ = true; then
                    if test $ndbm_ = true; then
                        AC_DEFINE(HAVE_GDBM_H, [1], [Define to 1 if you have both <gdbm.h> and <ndbm.h> header files.])
                    fi
                fi
        fi
    fi

    found=false
    libgdbm=""

    if test $have_db = true; then
      AC_CHECK_LIB(gdbm,dbm_open,found=true,found=false)
      AC_CHECK_LIB(gdbm,dbm_fetch,,found=false)
      AC_CHECK_LIB(gdbm,dbm_store,,found=false)
      AC_CHECK_LIB(gdbm,dbm_close,,found=false)

      if test $found = true; then
        libgdbm="${libgdbm} -lgdbm"
      else
	AC_CHECK_LIB(gdbm_compat,dbm_open,found=true,found=false,-lgdbm)
	AC_CHECK_LIB(gdbm_compat,dbm_fetch,,found=false,-lgdbm)
	AC_CHECK_LIB(gdbm_compat,dbm_store,,found=false,-lgdbm)
	AC_CHECK_LIB(gdbm_compat,dbm_close,,found="false",-lgdbm)

	if test $found = true ; then
	  libgdbm="${libgdbm} -lgdbm_compat -lgdbm"
	fi
      fi
    fi

    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

