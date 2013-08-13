# generated automatically by aclocal 1.11 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005, 2006, 2007, 2008, 2009  Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

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
m4_include([m4/multilib.m4])
m4_include([m4/package_acldev.m4])
m4_include([m4/package_aiodev.m4])
m4_include([m4/package_attrdev.m4])
m4_include([m4/package_dmapidev.m4])
m4_include([m4/package_gdbmdev.m4])
m4_include([m4/package_globals.m4])
m4_include([m4/package_ssldev.m4])
m4_include([m4/package_utilies.m4])
m4_include([m4/package_uuiddev.m4])
m4_include([m4/package_xfslibs.m4])
