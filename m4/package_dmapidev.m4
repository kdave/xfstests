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

AC_DEFUN([AC_PACKAGE_NEED_MAKEHANDLE_LIBDM],
  [ AC_CHECK_LIB(dm, dm_make_handle,, [
        echo
        echo 'FATAL ERROR: could not find a valid DMAPI base library.'
        echo 'Install the data migration API (dmapi) library package.'
        echo 'Alternatively, run "make install" from the dmapi source.'
        exit 1
    ])
    libdm="-ldm"
    test -f `pwd`/../dmapi/libdm/libdm.la && \
        libdm="`pwd`/../dmapi/libdm/libdm.la"
    test -f ${libexecdir}${libdirsuffix}/libdm.la && \
	libdm="${libexecdir}${libdirsuffix}/libdm.la"
    AC_SUBST(libdm)
  ])
