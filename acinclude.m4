dnl Copyright (C) 2016 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
AC_DEFUN([AC_PACKAGE_WANT_LINUX_FIEMAP_H],
  [ AC_CHECK_HEADERS([linux/fiemap.h], [ have_fiemap=true ], [ have_fiemap=false ])
    AC_SUBST(have_fiemap)
  ])

AC_DEFUN([AC_PACKAGE_WANT_LINUX_PRCTL_H],
  [ AC_CHECK_HEADERS([sys/prctl.h], [ have_prctl=true ], [ have_prctl=false ])
    AC_SUBST(have_prctl)
  ])

AC_DEFUN([AC_PACKAGE_WANT_LINUX_FS_H],
  [ AC_CHECK_HEADER([linux/fs.h])
  ])

AC_DEFUN([AC_PACKAGE_WANT_FALLOCATE],
  [ AC_MSG_CHECKING([for fallocate])
    AC_TRY_LINK([
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <linux/falloc.h> ],
      [ fallocate(0, 0, 0, 0); ],
      [ have_fallocate=true; AC_MSG_RESULT(yes) ],
      [ have_fallocate=false; AC_MSG_RESULT(no) ])
    AC_SUBST(have_fallocate)
  ])

AC_DEFUN([AC_PACKAGE_WANT_OPEN_BY_HANDLE_AT],
  [ AC_MSG_CHECKING([for open_by_handle_at])
    AC_TRY_LINK([
#define _GNU_SOURCE
#include <fcntl.h>
      ],
      [
          struct file_handle fh;
          open_by_handle_at(0, &fh, 0);
      ],
      [ have_open_by_handle_at=true; AC_MSG_RESULT(yes) ],
      [ have_open_by_handle_at=false; AC_MSG_RESULT(no) ])
    AC_SUBST(have_open_by_handle_at)
  ])
