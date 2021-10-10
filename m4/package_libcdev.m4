# 
# Check if we have a working fadvise system call
# 
AC_DEFUN([AC_HAVE_FADVISE],
  [ AC_MSG_CHECKING([for fadvise ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
    ]], [[
	posix_fadvise(0, 1, 0, POSIX_FADV_NORMAL);
    ]])],[have_fadvise=yes
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_fadvise)
  ])

# 
# Check if we have a working madvise system call
# 
AC_DEFUN([AC_HAVE_MADVISE],
  [ AC_MSG_CHECKING([for madvise ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <sys/mman.h>
    ]], [[
	posix_madvise(0, 0, MADV_NORMAL);
    ]])],[have_madvise=yes
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_madvise)
  ])

# 
# Check if we have a working mincore system call
# 
AC_DEFUN([AC_HAVE_MINCORE],
  [ AC_MSG_CHECKING([for mincore ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <sys/mman.h>
    ]], [[
	mincore(0, 0, 0);
    ]])],[have_mincore=yes
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_mincore)
  ])

# 
# Check if we have a working sendfile system call
# 
AC_DEFUN([AC_HAVE_SENDFILE],
  [ AC_MSG_CHECKING([for sendfile ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <sys/sendfile.h>
    ]], [[
         sendfile(0, 0, 0, 0);
    ]])],[have_sendfile=yes
	AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_sendfile)
  ])

#
# Check if we have a getmntent libc call (Linux)
#
AC_DEFUN([AC_HAVE_GETMNTENT],
  [ AC_MSG_CHECKING([for getmntent ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <mntent.h>
    ]], [[
         getmntent(0);
    ]])],[have_getmntent=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_getmntent)
  ])

#
# Check if we have a getmntinfo libc call (FreeBSD, Mac OS X)
#
AC_DEFUN([AC_HAVE_GETMNTINFO],
  [ AC_MSG_CHECKING([for getmntinfo ])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
    ]], [[
         getmntinfo(0, 0);
    ]])],[have_getmntinfo=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_getmntinfo)
  ])

#
#
# Check if we have a copy_file_range system call (Linux)
#
AC_DEFUN([AC_HAVE_COPY_FILE_RANGE],
  [ AC_MSG_CHECKING([for copy_file_range])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>
    ]], [[
         syscall(__NR_copy_file_range, 0, 0, 0, 0, 0, 0);
    ]])],[have_copy_file_range=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_copy_file_range)
  ])

