AC_DEFUN([AC_PACKAGE_NEED_UUID_H],
  [ AC_CHECK_HEADERS(uuid.h)
    if test $ac_cv_header_uuid_h = no; then
	AC_CHECK_HEADERS(uuid/uuid.h,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1])
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUIDCOMPARE],
  [ AC_CHECK_FUNCS(uuid_compare)
    if test $ac_cv_func_uuid_compare = no; then
	AC_CHECK_LIB(uuid, uuid_compare, [libuuid=/usr/lib/libuuid.a], [
	echo
	echo 'FATAL ERROR: could not find a valid UUID library.'
	echo 'Install the Universally Unique Identifiers library package.'
	exit 1])
    fi
    AC_SUBST(libuuid)
  ])

AC_DEFUN([AC_PACKAGE_CHECK_LIBUUID],
  [ test $pkg_platform = freebsd && libuuid=""
  ])
