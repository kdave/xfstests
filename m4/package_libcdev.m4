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

# Check if we have SEEK_DATA
AC_DEFUN([AC_HAVE_SEEK_DATA],
  [ AC_MSG_CHECKING([for SEEK_DATA])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
    ]], [[
         lseek(-1, 0, SEEK_DATA);
    ]])],[have_seek_data=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_seek_data)
  ])

# Check if we have nftw
AC_DEFUN([AC_HAVE_NFTW],
  [ AC_MSG_CHECKING([for nftw])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <stddef.h>
#include <ftw.h>
    ]], [[
         nftw("/", (int (*)(const char *, const struct stat *, int, struct FTW *))1, 0, FTW_ACTIONRETVAL);
    ]])],[have_nftw=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_nftw)
  ])

# Check if we have RLIMIT_NOFILE
AC_DEFUN([AC_HAVE_RLIMIT_NOFILE],
  [ AC_MSG_CHECKING([for RLIMIT_NOFILE])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/resource.h>
    ]], [[
         struct rlimit rlimit;

         rlimit.rlim_cur = 0;
         getrlimit(RLIMIT_NOFILE, &rlimit);
    ]])],[have_rlimit_nofile=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_rlimit_nofile)
  ])

# Check if we have FICLONE
AC_DEFUN([AC_HAVE_FICLONE],
  [ AC_MSG_CHECKING([for FICLONE])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/ioctl.h>
#include <linux/fs.h>
    ]], [[
	 ioctl(-1, FICLONE, -1);
    ]])],[have_ficlone=yes
       AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    AC_SUBST(have_ficlone)
  ])

# Check if we have -ftrivial-auto-var-init=zero
AC_DEFUN([AC_HAVE_TRIVIAL_AUTO_VAR_INIT],
  [ AC_MSG_CHECKING([if C compiler supports zeroing automatic vars])
    OLD_CFLAGS="$CFLAGS"
    TEST_CFLAGS="-ftrivial-auto-var-init=zero"
    CFLAGS="$CFLAGS $TEST_CFLAGS"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
        [AC_MSG_RESULT([yes])]
        [autovar_init_cflags=$TEST_CFLAGS],
        [AC_MSG_RESULT([no])])
    CFLAGS="${OLD_CFLAGS}"
    AC_SUBST(autovar_init_cflags)
  ])
