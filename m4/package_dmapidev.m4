AC_DEFUN([AC_PACKAGE_NEED_XFS_DMAPI_H],
  [ AC_CHECK_HEADERS([xfs/dmapi.h])
    if test "$ac_cv_header_xfs_dmapi_h" != yes; then
        echo
        echo 'FATAL ERROR: could not find a valid DMAPI library header.'
        echo 'Install the data migration API (dmapi) development package.'
        echo 'Alternatively, run "make install-dev" from the dmapi source.'
        exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_WANT_DMAPI],
  [ AC_CHECK_LIB(dm, dm_make_handle, [ have_dmapi=true ], [
	have_dmapi=false
        echo
        echo 'WARNING: could not find a valid DMAPI base library.'
	echo 'If you want DMAPI support please install the data migration'
	echo 'API (dmapi) library package. Alternatively, run "make install"'
	echo 'from the dmapi source.'
	echo
    ])
    libdm="-ldm"
    test -f ${libexecdir}${libdirsuffix}/libdm.la && \
	libdm="${libexecdir}${libdirsuffix}/libdm.la"
    AC_SUBST(libdm)
    AC_SUBST(have_dmapi)
  ])
