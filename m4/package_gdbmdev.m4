AC_DEFUN([AC_PACKAGE_WANT_GDBM],
  [
    have_db=
    AC_CHECK_HEADER(gdbm-ndbm.h,
      [
	have_db=true
        AC_DEFINE(HAVE_GDBM_NDBM_H, [1],
		  [Define to 1 if you have the <gdbm-ndbm.h> header file.])
      ])

    if test -z "$have_db"; then
      dnl gdbm-ndbm.h and gdbm/ndbm.h map to the same autoconf internal
      dnl variable.  We need to clear it or this test will be skipped
      dnl and the cached result from first test will be used.
      AS_UNSET([ac_cv_header_gdbm_ndbm_h])
      AC_CHECK_HEADER(gdbm/ndbm.h,
	[
	  have_db=true
	  AC_DEFINE(HAVE_GDBM_NDBM_H_, [1],
		    [Define to 1 if you have the <gdbm/ndbm.h> header file.])
	])
    fi

    if test -z "$have_db"; then
      AC_CHECK_HEADER(ndbm.h,
	[
	  have_db=true
	  AC_DEFINE(HAVE_NDBM_H, [1],
		    [Define to 1 if you have the <ndbm.h> header file.])
	])
    fi

    if test "$have_db" = true; then
      found=false
      AC_CHECK_LIB(gdbm, dbm_open, found=true, found=false)
      AC_CHECK_LIB(gdbm, dbm_fetch,, found=false)
      AC_CHECK_LIB(gdbm, dbm_store,, found=false)
      AC_CHECK_LIB(gdbm, dbm_close,, found=false)

      if test "$found" = true; then
        libgdbm="-lgdbm"
      else
	AC_CHECK_LIB(gdbm_compat, dbm_open, found=true, found=false, -lgdbm)
	AC_CHECK_LIB(gdbm_compat, dbm_fetch,, found=false, -lgdbm)
	AC_CHECK_LIB(gdbm_compat, dbm_store,, found=false, -lgdbm)
	AC_CHECK_LIB(gdbm_compat, dbm_close,, found="no", -lgdbm)

	if test "$found" = "true"; then
	  libgdbm="-lgdbm_compat -lgdbm"
	fi
      fi
    fi

    AC_SUBST(libgdbm)
    AC_SUBST(have_db)
  ])

