/* compile with: gcc -g -O0 -Wall -I/usr/include/xfs -o t_immutable t_immutable.c -lhandle -lacl -lattr */

/*
 *  t_immutable.c - hideous test suite for immutable/append-only flags.
 *
 *  Copyright (C) 2003 Ethan Benson
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define TEST_UTIME

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <utime.h>
#include <errno.h>
#include <grp.h>
#include <libgen.h>
#include <sys/acl.h>
#include <attr/xattr.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <xfs/xfs.h>
#include <xfs/handle.h>
#include <xfs/jdm.h>

#ifndef XFS_SUPER_MAGIC
#define XFS_SUPER_MAGIC 0x58465342
#endif

extern const char *__progname;

static int fsetflag(const char *path, int fd, int on, int immutable)
{
#ifdef FS_IOC_SETFLAGS
     int fsflags = 0;
     int fsfl;

     if (ioctl(fd, FS_IOC_GETFLAGS, &fsflags) < 0) {
	  close(fd);
	  return 1;
     }
     if (immutable)
	  fsfl = FS_IMMUTABLE_FL;
     else
	  fsfl = FS_APPEND_FL;
     if (on)
	  fsflags |= fsfl;
     else
	  fsflags &= ~fsfl;
     if (ioctl(fd, FS_IOC_SETFLAGS, &fsflags) < 0) {
	  close(fd);
	  return 1;
     }
     close(fd);
     return 0;
#else
     errno = EOPNOTSUPP;
     close(fd);
     return 1;
#endif
}

static int add_acl(const char *path, const char *acl_text)
{
     acl_t acl;
     int s_errno;
     int fd;

     if ((acl = acl_from_text(acl_text)) == NULL)
	  return 1;
     if ((fd = open(path, O_RDONLY)) == -1)
	  return 1;
     if (acl_set_fd(fd, acl))
	  if (errno != EOPNOTSUPP)
	       return 1;
     s_errno = errno;
     acl_free(acl);
     close(fd);
     errno = s_errno;
     return 0;
}

static int fadd_acl(int fd, const char *acl_text)
{
     acl_t acl;
     int s_errno;

     if ((acl = acl_from_text(acl_text)) == NULL)
	  perror("acl_from_text");
     if (acl_set_fd(fd, acl))
	  if (errno != EOPNOTSUPP)
	       return 1;
     s_errno = errno;
     acl_free(acl);
     errno = s_errno;
     return 0;
}

static int del_acl(const char *path)
{
     int fd;
     int s_errno;
     acl_t acl;
     static const char *acl_text = "u::rw-,g::rw-,o::rw-";

     if ((acl = acl_from_text(acl_text)) == NULL)
	  return 1;
     if ((fd = open(path, O_RDONLY)) == -1)
	  return 1;
     if (acl_set_fd(fd, acl))
	  if (errno != EOPNOTSUPP)
	       return 1;
     s_errno = errno;
     acl_free(acl);
     close(fd);
     errno = s_errno;
     return 0;
}

static int test_immutable(const char *dir)
{
     int fd;
     char *buf;
     char *path;
     char *linkpath;
     int fail = 0;
     struct utimbuf tbuf;
     struct stat st;
     struct statfs stfs;
     static const char *scribble = "scribbled by tester\n";
     static const char *acl_text = "u::rwx,g::rwx,o::rwx,u:daemon:rwx,m::rwx";

     tbuf.actime = 0;
     tbuf.modtime = 0;

     if (statfs(dir, &stfs) == -1) {
	  perror("statfs failed");
	  return 1;
     }

     asprintf(&path, "%s/immutable.f", dir);
     errno = 0;
     if ((fd = open(path, O_RDWR)) != -1) {
	  fprintf(stderr, "open(%s, O_RDWR) did not fail\n", path);
	  fail++;
	  close(fd);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "open(%s, O_RDWR) did not set errno == EACCES or EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY)) != -1) {
	  fprintf(stderr, "open(%s, O_WRONLY) did not fail\n", path);
	  fail++;
	  close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_RDWR|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_WRONLY|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY|O_TRUNC did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_APPEND)) != -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY|O_APPEND)) != -1) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_APPEND|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY|O_APPEND|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     if (stfs.f_type == XFS_SUPER_MAGIC && !getuid()) {
	  jdm_fshandle_t *fshandle;
	  xfs_bstat_t bstat;
	  xfs_fsop_bulkreq_t  bulkreq;
	  xfs_ino_t ino;
	  char *dirpath;

	  dirpath = strdup(path); /* dirname obnoxiously modifies its arg */
	  if ((fshandle = jdm_getfshandle(dirname(dirpath))) == NULL) {
	       perror("jdm_getfshandle");
	       return 1;
	  }
	  free(dirpath);

	  if (stat(path, &st) != 0) {
	       perror("stat");
	       return 1;
	  }

	  ino = st.st_ino;

	  bulkreq.lastip = (__u64 *)&ino;
	  bulkreq.icount = 1;
	  bulkreq.ubuffer = &bstat;
	  bulkreq.ocount = NULL;

	  if ((fd = open(path, O_RDONLY)) == -1) {
	       perror("open");
	       return 1;
	  }

	  if (ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE, &bulkreq) == -1) {
	       perror("ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE");
	       close(fd);
	       return 1;
	  }
	  close(fd);

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       perror("jdm_open");
	       fprintf(stderr, "jdm_open(%s, O_RDWR) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_TRUNC did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_APPEND)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_APPEND)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_APPEND|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_APPEND|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EACCES && errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not set errno == EACCES or EPERM\n", path);
	       fail++;
	  }
     }

     errno = 0;
     if (truncate(path, 0) != -1) {
	  fprintf(stderr, "truncate(%s, 0) did not fail\n", path);
	  fail++;
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "truncate(%s, 0) did not set errno == EACCES or EPERM\n", path);
	  fail++;
     }

#ifdef TEST_UTIME
     errno = 0;
     if (utime(path, &tbuf) != -1) {
	  fprintf(stderr, "utime(%s, <epoch>) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "utime(%s, <epoch>) did not set errno == EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if (utime(path, NULL) != -1) {
          fprintf(stderr, "utime(%s, NULL) did not fail\n", path);
          fail++;
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "utime(%s, NULL) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }
#endif /* TEST_UTIME */

     asprintf(&linkpath, "%s/immutable.f.hardlink", dir);
     errno = 0;
     if (link(path, linkpath) != -1) {
	  fprintf(stderr, "link(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  unlink(linkpath);
     } else if (errno != EPERM) {
	  fprintf(stderr, "link(%s, %s) did not set errno == EPERM\n", path, linkpath);
	  fail++;
     }
     free(linkpath);

     if (!getuid()) { /* these would fail if not root anyway */
	  errno = 0;
	  if (chmod(path, 7777) != -1) {
	       fprintf(stderr, "chmod(%s, 7777) did not fail\n", path);
	       fail++;
	       chmod(path, 0666);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chmod(%s, 7777) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (chown(path, 1, 1) != -1) {
	       fprintf(stderr, "chown(%s, 1, 1) did not fail\n", path);
	       fail++;
	       chown(path, 0, 0);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chown(%s, 1, 1) did not set errno to EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (del_acl(path) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "del_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "del_acl(%s) did not set errno == EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (add_acl(path, acl_text) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "add_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "add_acl(%s) did not set errno == EPERM\n", path);
               fail++;
          }

          if (setxattr(path, "trusted.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
               fail++;
          }
          if (setxattr(path, "trusted.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
               fail++;
          }

          if (removexattr(path, "trusted.test") != -1) {
               fprintf(stderr, "removexattr(%s, trusted.test) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "removexattr(%s, trusted.test) did not set errno == EPERM\n", path);
               fail++;
          }
     }

     if (setxattr(path, "user.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (setxattr(path, "user.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (removexattr(path, "user.test") != -1) {
	  fprintf(stderr, "removexattr(%s, user.test) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
	  fprintf(stderr,
		  "removexattr(%s, user.test) did not set errno == EPERM\n", path);
	  fail++;
     }

     asprintf(&linkpath, "%s/immutable.f.newname", dir);
     errno = 0;
     if (rename(path, linkpath) != -1) {
	  fprintf(stderr, "rename(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  rename(linkpath, path);
     } else if (errno != EPERM) {
	  fprintf(stderr, "rename(%s, %s) did not set errno == EPERM\n", path, linkpath);
	  fail++;
     }
     free(linkpath);

     if ((fd = open(path, O_RDONLY)) == -1) {
	  fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  if (fstat(fd, &st) == -1) {
	       perror("fstat");
	       fail++;
	  } else if (st.st_size) {
	       if ((buf = malloc(st.st_size)) == NULL)
		    perror("malloc");
	       else {
		    if (lseek(fd, 0, SEEK_SET) == -1) {
			 perror("lseek(fd, 0, SEEK_SET) failed");
			 fail++;
		    }
		    if (read(fd, buf, st.st_size) != st.st_size) {
			 perror("read failed");
			 fail++;
		    }
		    free(buf);
	       }
	  }
	  close(fd);
     }

     errno = 0;
     if (unlink(path) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "unlink(%s) did not set errno == EPERM\n", path);
	  fail ++;
     }

     free(path);
     asprintf(&path, "%s/immutable.d/file", dir);
     if ((fd = open(path, O_RDWR)) == -1) {
	  fprintf(stderr, "open(%s, O_RDWR) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
	       fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble), strerror(errno));
	       fail++;
	  }
	  close(fd);
     }
     if (!getuid()) {
	  if (chmod(path, 0777) == -1) {
	       fprintf(stderr, "chmod(%s, 0777) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chmod(path, 0666);
	  if (chown(path, 1, 1) == -1) {
	       fprintf(stderr, "chown(%s, 1, 1) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chown(path, 0, 0);
     }

     asprintf(&linkpath, "%s/immutable.d/file.link", dir);
     errno = 0;
     if (link(path, linkpath) != -1) {
	  fprintf(stderr, "link(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  unlink(linkpath);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "link(%s, %s) did not set errno == EACCES or EPERM\n", path, linkpath);
	  fail++;
     }
     if (symlink(path, linkpath) != -1) {
	  fprintf(stderr, "symlink(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  unlink(linkpath);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "symlink(%s, %s) did not set errno == EACCES or EPERM\n", path, linkpath);
          fail++;
     }
     free(linkpath);
     asprintf(&linkpath, "%s/immutable.d/file.newname", dir);
     if (rename(path, linkpath) != -1) {
	  fprintf(stderr, "rename(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  rename(linkpath, path);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "rename(%s, %s) did not set errno == EACCES or EPERM\n", path, linkpath);
          fail++;
     }
     free(linkpath);

     if (unlink(path) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "unlink(%s) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/immutable.d/newfile", dir);
     errno = 0;
     if ((fd = open(path, O_RDWR|O_CREAT, 0666)) != -1) {
	  fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) did not fail\n", path);
	  fail++;
	  unlink(path);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) did not set errno == EACCES or EPERM\n", path);
	  fail++;
     }
     if (!getuid()) {
	  if (stat("/dev/null", &st) != -1) {
	       if (mknod(path, S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, st.st_rdev) != -1) {
		    fprintf(stderr, "mknod(%s, S_IFCHR|0666, %lld) did not fail\n", path, (long long int)st.st_rdev);
		    fail++;
		    unlink(path);
	       } else if (errno != EACCES && errno != EPERM) {
		    fprintf(stderr, "mknod(%s, S_IFCHR|0666, %lld) did not set errno == EACCES or EPERM\n", path, (long long int)st.st_rdev);
		    fail++;
	       }
	  }
     }

     free(path);
     asprintf(&path, "%s/immutable.d/newdir", dir);
     errno = 0;
     if (mkdir(path, 0777) != -1) {
	  fprintf(stderr, "mkdir(%s, 0777) did not fail\n", path);
	  fail++;
	  rmdir(path);
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "mkdir(%s, 0777) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/immutable.d/dir/newfile-%d", dir, getuid());
     if ((fd = open(path, O_RDWR|O_CREAT, 0666)) == -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) failed: %s\n", path, strerror(errno));
          fail++;
     } else {
          if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
               fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble), strerror(errno));
               fail++;
          }
          close(fd);
     }
     if (!getuid()) {
	  if (chmod(path, 0700) == -1) {
	       fprintf(stderr, "chmod(%s, 0700) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chmod(path, 0666);

          if (chown(path, 1, 1) == -1) {
               fprintf(stderr, "chown(%s, 1, 1) failed: %s\n", path, strerror(errno));
               fail++;
          } else
               chown(path, 0, 0);
     }

     free(path);
     asprintf(&path, "%s/immutable.d/dir", dir);
     errno = 0;
     if (rmdir(path) != -1) {
	  fprintf(stderr, "rmdir(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EACCES && errno != EPERM) {
	  fprintf(stderr, "rmdir(%s) did not set errno == EACCES or EPERM\n", path);
	  fail++;
     }

     free(path);
     asprintf(&path, "%s/immutable.d", dir);

#ifdef TEST_UTIME
     errno = 0;
     if (utime(path, &tbuf) != -1) {
	  fprintf(stderr, "utime(%s, <epoch>) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "utime(%s, <epoch>) did not set errno == EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if (utime(path, NULL) != -1) {
          fprintf(stderr, "utime(%s, NULL) did not fail\n", path);
          fail++;
     } else if (errno != EACCES && errno != EPERM) {
          fprintf(stderr, "utime(%s, NULL) did not set errno == EACCES or EPERM\n", path);
          fail++;
     }
#endif /* TEST_UTIME */

     if (!getuid()) { /* these would fail if not root anyway */
	  errno = 0;
	  if (chmod(path, 7777) != -1) {
	       fprintf(stderr, "chmod(%s, 7777) did not fail\n", path);
	       fail++;
	       chmod(path, 0666);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chmod(%s, 7777) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (chown(path, 1, 1) != -1) {
	       fprintf(stderr, "chown(%s, 1, 1) did not fail\n", path);
	       fail++;
	       chown(path, 0, 0);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chown(%s, 1, 1) did not set errno to EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (del_acl(path) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "del_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "del_acl(%s) did not set errno == EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (add_acl(path, acl_text) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "add_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "add_acl(%s) did not set errno == EPERM\n", path);
               fail++;
          }

	  if (setxattr(path, "trusted.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
	       fprintf(stderr, "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
	       fail++;
	  } else if (errno != EPERM && errno != EOPNOTSUPP) {
	       fprintf(stderr,
		       "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
	       fail++;
	  }
	  if (setxattr(path, "trusted.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
	       fprintf(stderr, "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
	       fail++;
	  } else if (errno != EPERM && errno != EOPNOTSUPP) {
	       fprintf(stderr,
		       "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
	       fail++;
	  }
          if (removexattr(path, "trusted.test") != -1) {
               fprintf(stderr, "removexattr(%s, trusted.test) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "removexattr(%s, trusted.test) did not set errno == EPERM\n", path);
               fail++;
          }
     }

     if (setxattr(path, "user.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
	  fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
	  fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
	  fail++;
     }
     if (setxattr(path, "user.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (removexattr(path, "user.test") != -1) {
          fprintf(stderr, "removexattr(%s, user.test) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr,
                  "removexattr(%s, user.test) did not set errno == EPERM\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/empty-immutable.d", dir);
     errno = 0;
     if (rmdir(path) != -1) {
          fprintf(stderr, "rmdir(%s) did not fail\n", path);
          fail++;
     } else if (errno != EPERM) {
          fprintf(stderr, "rmdir(%s) did not set errno == EPERM\n", path);
          fail++;
     }

     free(path);
     return fail;
}

static int test_append(const char *dir)
{
     int fd;
     char *buf;
     char *orig = NULL;
     char *path;
     char *linkpath;
     off_t origsize = 0;
     int fail = 0;
     struct utimbuf tbuf;
     struct stat st;
     struct statfs stfs;
     static const char *acl_text = "u::rwx,g::rwx,o::rwx,u:daemon:rwx,m::rwx";
     static const char *scribble = "scribbled by tester\n";
     static const char *scribble2 = "scribbled by tester\nscribbled by tester\n";
     static const char *scribble4 = "scribbled by tester\nscribbled by tester\n"
	  "scribbled by tester\nscribbled by tester\n";

     tbuf.actime = 0;
     tbuf.modtime = 0;

     if (statfs(dir, &stfs) == -1) {
          perror("statfs failed");
          return 1;
     }

     asprintf(&path, "%s/append-only.f", dir);
     errno = 0;
     if ((fd = open(path, O_RDWR)) != -1) {
	  fprintf(stderr, "open(%s, O_RDWR) did not fail\n", path);
	  fail++;
	  close(fd);
     } else if (errno != EPERM) {
	  fprintf(stderr, "open(%s, O_RDWR) did not set errno == EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY)) != -1) {
	  fprintf(stderr, "open(%s, O_WRONLY) did not fail\n", path);
	  fail++;
	  close(fd);
     } else if (errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY) did not set errno == EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EPERM) {
          fprintf(stderr, "open(%s, O_RDWR|O_TRUNC) did not set errno == EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_WRONLY|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY|O_TRUNC did not set errno == EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_APPEND|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EPERM) {
          fprintf(stderr, "open(%s, O_RDWR|O_APPEND|O_TRUNC) did not set errno == EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_WRONLY|O_APPEND|O_TRUNC)) != -1) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not fail\n", path);
          fail++;
          close(fd);
     } else if (errno != EPERM) {
          fprintf(stderr, "open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not set errno == EPERM\n", path);
          fail++;
     }

     errno = 0;
     if ((fd = open(path, O_RDWR|O_APPEND)) == -1) {
	  fprintf(stderr, "open(%s, O_RDWR|O_APPEND) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	     if (ftruncate(fd, 0) != -1) {
		     fprintf(stderr, "ftruncate(%s, 0) did not fail\n", path);
		     fail++;
	     } else if (errno != EPERM) {
		     fprintf(stderr, "ftruncate(%s, 0) did not set errno == EPERM\n", path);
		     fail++;
	     }
	     close(fd);
     }

     errno = 0;
     if (truncate(path, 0) != -1) {
	  fprintf(stderr, "truncate(%s, 0) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "truncate(%s, 0) did not set errno == EPERM\n", path);
	  fail++;
     }

     if (stfs.f_type == XFS_SUPER_MAGIC && !getuid()) {
	  jdm_fshandle_t *fshandle;
	  xfs_bstat_t bstat;
	  xfs_fsop_bulkreq_t  bulkreq;
	  xfs_ino_t ino;
	  char *dirpath;

	  dirpath = strdup(path); /* dirname obnoxiously modifies its arg */
	  if ((fshandle = jdm_getfshandle(dirname(dirpath))) == NULL) {
	       perror("jdm_getfshandle");
	       return 1;
	  }
	  free(dirpath);

	  if (stat(path, &st) != 0) {
	       perror("stat");
	       return 1;
	  }

	  ino = st.st_ino;

	  bulkreq.lastip = (__u64 *)&ino;
	  bulkreq.icount = 1;
	  bulkreq.ubuffer = &bstat;
	  bulkreq.ocount = NULL;

	  if ((fd = open(path, O_RDONLY)) == -1) {
	       perror("open");
	       return 1;
	  }

	  if (ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE, &bulkreq) == -1) {
	       perror("ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE");
	       close(fd);
	       return 1;
	  }
	  close(fd);

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       perror("jdm_open");
	       fprintf(stderr, "jdm_open(%s, O_RDWR) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_TRUNC) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_TRUNC did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_APPEND|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND|O_TRUNC) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_APPEND|O_TRUNC)) != -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not fail\n", path);
	       fail++;
	       close(fd);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND|O_TRUNC) did not set errno == EPERM\n", path);
	       fail++;
	  }
     }

#ifdef TEST_UTIME
     errno = 0;
     if (utime(path, &tbuf) != -1) {
	  fprintf(stderr, "utime(%s, <epoch>) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "utime(%s, <epoch>) did not set errno == EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if (utime(path, NULL) == -1) {
          fprintf(stderr, "utime(%s, NULL) failed\n", path);
          fail++;
     }
#endif /* TEST_UTIME */

     asprintf(&linkpath, "%s/append-only.f.hardlink", dir);
     errno = 0;
     if (link(path, linkpath) != -1) {
	  fprintf(stderr, "link(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  unlink(linkpath);
     } else if (errno != EPERM) {
	  fprintf(stderr, "link(%s, %s) did not set errno == EPERM\n", path, linkpath);
	  fail++;
     }
     free(linkpath);

     if (!getuid()) { /* these would fail if not root anyway */
	  errno = 0;
	  if (chmod(path, 7777) != -1) {
	       fprintf(stderr, "chmod(%s, 7777) did not fail\n", path);
	       fail++;
	       chmod(path, 0666);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chmod(%s, 7777) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (chown(path, 1, 1) != -1) {
	       fprintf(stderr, "chown(%s, 1, 1) did not fail\n", path);
	       fail++;
	       chown(path, 0, 0);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chown(%s, 1, 1) did not set errno to EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (del_acl(path) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "del_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "del_acl(%s) did not set errno == EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (add_acl(path, acl_text) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "add_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "add_acl(%s) did not set errno == EPERM\n", path);
               fail++;
          }

          if (setxattr(path, "trusted.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
               fail++;
          }
          if (setxattr(path, "trusted.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
               fail++;
          }
          if (removexattr(path, "trusted.test") != -1) {
               fprintf(stderr, "removexattr(%s, trusted.test) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "removexattr(%s, trusted.test) did not set errno == EPERM\n", path);
               fail++;
          }
     }

     if (setxattr(path, "user.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (setxattr(path, "user.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (removexattr(path, "user.test") != -1) {
          fprintf(stderr, "removexattr(%s, user.test) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr,
                  "removexattr(%s, user.test) did not set errno == EPERM\n", path);
          fail++;
     }

     asprintf(&linkpath, "%s/append-only.f.newname", dir);
     errno = 0;
     if (rename(path, linkpath) != -1) {
	  fprintf(stderr, "rename(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  rename(linkpath, path);
     } else if (errno != EPERM) {
	  fprintf(stderr, "rename(%s, %s) did not set errno == EPERM\n", path, linkpath);
	  fail++;
     }
     free(linkpath);

     if ((fd = open(path, O_RDONLY)) == -1) {
	  fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  if (fstat(fd, &st) == -1) {
	       perror("fstat");
	       fail++;
	  } else if (st.st_size) {
	       origsize = st.st_size;
	       if ((orig = malloc(st.st_size)) == NULL)
		    perror("malloc");
	       else {
		    if (lseek(fd, 0, SEEK_SET) == -1) {
			 perror("lseek(fd, 0, SEEK_SET) failed");
			 fail++;
		    }
		    if (read(fd, orig, st.st_size) != st.st_size) {
			 perror("read failed");
			 fail++;
		    }
	       }
	  }
	  close(fd);
     }

     if ((fd = open(path, O_WRONLY|O_APPEND)) == -1) {
	  fprintf(stderr, "open(%s, O_WRONLY|O_APPEND) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  lseek(fd, 0, SEEK_SET); /* this is silently ignored */
	  if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
	       fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
		       strerror(errno));
	       fail++;
	  }
	  lseek(fd, origsize, SEEK_SET); /* this is silently ignored */
          if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
               fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
                       strerror(errno));
               fail++;
          }
	  close(fd);
	  if ((fd = open(path, O_RDONLY)) == -1) { /* now we check to make sure lseek() ignored us */
	       fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else {
	       if (lseek(fd, 0, SEEK_SET) == -1) {
		    fprintf(stderr, "lseek(%s, 0, SEEK_SET) failed: %s\n", path, strerror(errno));
		    fail++;
	       } else if (origsize) {
		    if ((buf = malloc(origsize)) == NULL)
			 perror("malloc");
		    else {
			 if (read(fd, buf, origsize) == -1) {
			      fprintf(stderr, "read(%s, &buf, %ld) failed: %s\n", path, (long)origsize, strerror(errno));
			      fail++;
			 } else {
			      if (memcmp(orig, buf, origsize)) {
				   fprintf(stderr, "existing data in append-only.f was overwritten\n");
				   fail++;
			      }
			 }
			 free(buf);
		    }
	       }
	       if (lseek(fd, origsize, SEEK_SET) == -1) {
                    fprintf(stderr, "lseek(%s, %ld, SEEK_SET) failed: %s\n", path, (long)origsize, strerror(errno));
                    fail++;
               } else {
		    if ((buf = malloc(strlen(scribble2))) == NULL)
                         perror("malloc");
                    else {
                         if (read(fd, buf, strlen(scribble2)) == -1) {
                              fprintf(stderr, "read(%s, &buf, %d) failed: %s\n", path,
				      (int)strlen(scribble2), strerror(errno));
                              fail++;
                         } else {
                              if (memcmp(scribble2, buf, strlen(scribble2))) {
                                   fprintf(stderr, "existing data in append-only.f was overwritten\n");
                                   fail++;
                              }
                         }
                         free(buf);
                    }
		    close(fd);
	       }
	  }
     }

     if ((fd = open(path, O_RDWR|O_APPEND)) == -1) {
	  fprintf(stderr, "open(%s, O_RDWR|O_APPEND) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  lseek(fd, 0, SEEK_SET); /* this is silently ignored */
	  if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
	       fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
		       strerror(errno));
	       fail++;
	  }
	  lseek(fd, origsize, SEEK_SET); /* this is silently ignored */
          if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
               fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
                       strerror(errno));
               fail++;
          }
	  close(fd);
	  if ((fd = open(path, O_RDONLY)) == -1) { /* now we check to make sure lseek() ignored us */
	       fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else {
	       if (lseek(fd, 0, SEEK_SET) == -1) {
		    fprintf(stderr, "lseek(%s, 0, SEEK_SET) failed: %s\n", path, strerror(errno));
		    fail++;
	       } else if (origsize) {
		    if ((buf = malloc(origsize)) == NULL)
			 perror("malloc");
		    else {
			 if (read(fd, buf, origsize) == -1) {
			      fprintf(stderr, "read(%s, &buf, %ld) failed: %s\n", path, (long)origsize, strerror(errno));
			      fail++;
			 } else {
			      if (memcmp(orig, buf, origsize)) {
				   fprintf(stderr, "existing data in append-only.f was overwritten\n");
				   fail++;
			      }
			 }
			 free(buf);
			 free(orig);
		    }
	       }
	       if (lseek(fd, origsize, SEEK_SET) == -1) {
                    fprintf(stderr, "lseek(%s, %ld, SEEK_SET) failed: %s\n", path, (long)origsize, strerror(errno));
                    fail++;
               } else if (scribble4) {
		    if ((buf = malloc(strlen(scribble4))) == NULL)
                         perror("malloc");
                    else {
                         if (read(fd, buf, strlen(scribble4)) == -1) {
                              fprintf(stderr, "read(%s, &buf, %d) failed: %s\n", path,
				      (int)strlen(scribble4), strerror(errno));
                              fail++;
                         } else {
                              if (memcmp(scribble4, buf, strlen(scribble4))) {
                                   fprintf(stderr, "existing data in append-only.f was overwritten\n");
                                   fail++;
                              }
                         }
                         free(buf);
                    }
		    close(fd);
	       }
	  }
     }

     if (stfs.f_type == XFS_SUPER_MAGIC && !getuid()) {
          jdm_fshandle_t *fshandle;
          xfs_bstat_t bstat;
          xfs_fsop_bulkreq_t  bulkreq;
          xfs_ino_t ino;
          char *dirpath;

	  if ((fd = open(path, O_RDONLY)) == -1) {
	       fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else {
	       if (fstat(fd, &st) == -1) {
		    perror("fstat");
		    fail++;
	       } else if (st.st_size) {
		    origsize = st.st_size;
		    if ((orig = malloc(st.st_size)) == NULL)
			 perror("malloc");
		    else {
			 if (lseek(fd, 0, SEEK_SET) == -1) {
			      perror("lseek(fd, 0, SEEK_SET) failed");
			      fail++;
			 }
			 if (read(fd, orig, st.st_size) != st.st_size) {
			      perror("read failed");
			      fail++;
			 }
		    }
	       }
	       close(fd);
	  }

          dirpath = strdup(path); /* dirname obnoxiously modifies its arg */
          if ((fshandle = jdm_getfshandle(dirname(dirpath))) == NULL) {
               perror("jdm_getfshandle");
               return 1;
          }
          free(dirpath);

          if (stat(path, &st) != 0) {
               perror("stat");
               return 1;
          }

          ino = st.st_ino;

          bulkreq.lastip = (__u64 *)&ino;
          bulkreq.icount = 1;
          bulkreq.ubuffer = &bstat;
          bulkreq.ocount = NULL;

          if ((fd = open(path, O_RDONLY)) == -1) {
               perror("open");
               return 1;
          }

          if (ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE, &bulkreq) == -1) {
               perror("ioctl(fd, XFS_IOC_FSBULKSTAT_SINGLE");
               close(fd);
               return 1;
          }
          close(fd);

	  if ((fd = jdm_open(fshandle, &bstat, O_WRONLY|O_APPEND)) == -1) {
	       fprintf(stderr, "jdm_open(%s, O_WRONLY|O_APPEND) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else {
	       lseek(fd, 0, SEEK_SET); /* this is silently ignored */
	       if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
		    fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
			    strerror(errno));
		    fail++;
	       }
	       lseek(fd, origsize, SEEK_SET); /* this is silently ignored */
	       if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
		    fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
			    strerror(errno));
		    fail++;
	       }
	       close(fd);
	       if ((fd = jdm_open(fshandle, &bstat, O_RDONLY)) == -1) { /* now we check to make sure lseek() ignored us */
		    fprintf(stderr, "jdm_open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
		    fail++;
	       } else {
		    if (lseek(fd, 0, SEEK_SET) == -1) {
			 fprintf(stderr, "lseek(%s, 0, SEEK_SET) failed: %s\n", path, strerror(errno));
			 fail++;
		    } else if (origsize) {
			 if ((buf = malloc(origsize)) == NULL)
			      perror("malloc");
			 else {
			      if (read(fd, buf, origsize) == -1) {
				   fprintf(stderr, "read(%s, &buf, %ld) failed: %s\n", path, (long)origsize, strerror(errno));
				   fail++;
			      } else {
				   if (memcmp(orig, buf, origsize)) {
					fprintf(stderr, "existing data in append-only.f was overwritten\n");
					fail++;
				   }
			      }
			      free(buf);
			 }
		    }
		    if (lseek(fd, origsize, SEEK_SET) == -1) {
			 fprintf(stderr, "lseek(%s, %ld, SEEK_SET) failed: %s\n", path, (long)origsize, strerror(errno));
			 fail++;
		    } else if (strlen(scribble2)) {
			 if ((buf = malloc(strlen(scribble2))) == NULL)
			      perror("malloc");
			 else {
			      if (read(fd, buf, strlen(scribble2)) == -1) {
				   fprintf(stderr, "read(%s, &buf, %d) failed: %s\n", path,
					   (int)strlen(scribble2), strerror(errno));
				   fail++;
			      } else {
				   if (memcmp(scribble2, buf, strlen(scribble2))) {
					fprintf(stderr, "existing data in append-only.f was overwritten\n");
					fail++;
				   }
			      }
			      free(buf);
			 }
			 close(fd);
		    }
	       }
	  }

	  if ((fd = jdm_open(fshandle, &bstat, O_RDWR|O_APPEND)) == -1) {
	       fprintf(stderr, "jdm_open(%s, O_RDWR|O_APPEND) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else {
	       lseek(fd, 0, SEEK_SET); /* this is silently ignored */
	       if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
		    fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
			    strerror(errno));
		    fail++;
	       }
	       lseek(fd, origsize, SEEK_SET); /* this is silently ignored */
	       if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
		    fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble),
			    strerror(errno));
		    fail++;
	       }
	       close(fd);
	       if ((fd = jdm_open(fshandle, &bstat, O_RDONLY)) == -1) { /* now we check to make sure lseek() ignored us */
		    fprintf(stderr, "jdm_open(%s, O_RDONLY) failed: %s\n", path, strerror(errno));
		    fail++;
	       } else {
		    if (lseek(fd, 0, SEEK_SET) == -1) {
			 fprintf(stderr, "lseek(%s, 0, SEEK_SET) failed: %s\n", path, strerror(errno));
			 fail++;
		    } else if (origsize) {
			 if ((buf = malloc(origsize)) == NULL)
			      perror("malloc");
			 else {
			      if (read(fd, buf, origsize) == -1) {
				   fprintf(stderr, "read(%s, &buf, %ld) failed: %s\n", path, (long)origsize, strerror(errno));
				   fail++;
			      } else {
				   if (memcmp(orig, buf, origsize)) {
					fprintf(stderr, "existing data in append-only.f was overwritten\n");
					fail++;
				   }
			      }
			      free(buf);
			      free(orig);
			 }
		    }
		    if (lseek(fd, origsize, SEEK_SET) == -1) {
			 fprintf(stderr, "lseek(%s, %ld, SEEK_SET) failed: %s\n", path, (long)origsize, strerror(errno));
			 fail++;
		    } else if (strlen(scribble4)) {
			 if ((buf = malloc(strlen(scribble4))) == NULL)
			      perror("malloc");
			 else {
			      if (read(fd, buf, strlen(scribble4)) == -1) {
				   fprintf(stderr, "read(%s, &buf, %d) failed: %s\n", path,
					   (int)strlen(scribble4), strerror(errno));
				   fail++;
			      } else {
				   if (memcmp(scribble4, buf, strlen(scribble4))) {
					fprintf(stderr, "existing data in append-only.f was overwritten\n");
					fail++;
				   }
			      }
			      free(buf);
			 }
			 close(fd);
		    }
	       }
	  }
     }

     errno = 0;
     if (unlink(path) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "unlink(%s) did not set errno == EPERM\n", path);
	  fail ++;
     }

     free(path);
     asprintf(&path, "%s/append-only.d/file", dir);
     if ((fd = open(path, O_RDWR)) == -1) {
	  fprintf(stderr, "open(%s, O_RDWR) failed: %s\n", path, strerror(errno));
	  fail++;
     } else {
	  if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
	       fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble), strerror(errno));
	       fail++;
	  }
	  close(fd);
     }
     if (!getuid()) {
	  if (chmod(path, 0777) == -1) {
	       fprintf(stderr, "chmod(%s, 0777) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chmod(path, 0666);
	  if (chown(path, 1, 1) == -1) {
	       fprintf(stderr, "chown(%s, 1, 1) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chown(path, 0, 0);
     }

     asprintf(&linkpath, "%s/append-only.d/file.link-%d", dir, getuid());
     errno = 0;
     if (link(path, linkpath) == -1) {
	  fprintf(stderr, "link(%s, %s) failed: %s\n", path, linkpath, strerror(errno));
	  fail++;
     } else if (unlink(linkpath) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", linkpath);
	  fail++;
     }
     free(linkpath);
     asprintf(&linkpath, "%s/append-only.d/file.symlink-%d", dir, getuid());
     if (symlink(path, linkpath) == -1) {
	  fprintf(stderr, "symlink(%s, %s) failed: %s\n", path, linkpath, strerror(errno));
	  fail++;
     } else if (unlink(linkpath) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", linkpath);
          fail++;
     }

     free(linkpath);
     asprintf(&linkpath, "%s/append-only.d/file.newname", dir);
     if (rename(path, linkpath) != -1) {
	  fprintf(stderr, "rename(%s, %s) did not fail\n", path, linkpath);
	  fail++;
	  rename(linkpath, path);
     } else if (errno != EPERM) {
	  fprintf(stderr, "rename(%s, %s) did not set errno == EPERM\n", path, linkpath);
          fail++;
     }
     free(linkpath);

     if (unlink(path) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "unlink(%s) did not set errno == EPERM\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/append-only.d/newfile-%d", dir, getuid());
     errno = 0;
     if ((fd = open(path, O_RDWR|O_CREAT, 0666)) == -1) {
	  fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) failed: %s\n", path, strerror(errno));
	  fail++;
     } else if (unlink(path) != -1) {
	  fprintf(stderr, "unlink(%s) did not fail\n", path);
	  fail++;
	  close(fd);
     } else
	  close(fd);

     if (!getuid()) {
	  free(path);
	  asprintf(&path, "%s/append-only.d/newdev-%d", dir, getuid());
	  if (stat("/dev/null", &st) != -1) {
	       if (mknod(path, S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, st.st_rdev) == -1) {
		    fprintf(stderr, "mknod(%s, S_IFCHR|0666, %lld) failed: %s\n", path, (long long int)st.st_rdev, strerror(errno));
		    fail++;
	       } else if (unlink(path) != -1) {
		    fprintf(stderr, "unlink(%s) did not fail\n", path);
		    fail++;
	       }
	  }
     }

     free(path);
     asprintf(&path, "%s/append-only.d/newdir-%d", dir, getuid());
     if (mkdir(path, 0777) == -1) {
	  fprintf(stderr, "mkdir(%s, 0777) failed: %s\n", path, strerror(errno));
	  fail++;
     } else if (rmdir(path) != -1) {
	  fprintf(stderr, "rmdir(%s) did not fail\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/append-only.d/newdir-%d/newfile-%d", dir, getuid(), getuid());
     if ((fd = open(path, O_RDWR|O_CREAT, 0666)) == -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) failed: %s\n", path, strerror(errno));
          fail++;
     } else {
          if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
               fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble), strerror(errno));
               fail++;
          }
          close(fd);
     }
     if (!getuid()) {
	  if (chmod(path, 0700) == -1) {
	       fprintf(stderr, "chmod(%s, 0700) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chmod(path, 0666);

          if (chown(path, 1, 1) == -1) {
               fprintf(stderr, "chown(%s, 1, 1) failed: %s\n", path, strerror(errno));
               fail++;
          } else
               chown(path, 0, 0);
     }

     free(path);
     asprintf(&path, "%s/append-only.d/dir/newfile-%d", dir, getuid());
     if ((fd = open(path, O_RDWR|O_CREAT, 0666)) == -1) {
          fprintf(stderr, "open(%s, O_RDWR|O_CREAT, 0666) failed: %s\n", path, strerror(errno));
          fail++;
     } else {
          if (write(fd, scribble, strlen(scribble)) != strlen(scribble)) {
               fprintf(stderr, "write(%s, %s, %d) failed: %s\n", path, scribble, (int)strlen(scribble), strerror(errno));
               fail++;
          }
          close(fd);
     }
     if (!getuid()) {
	  if (chmod(path, 0700) == -1) {
	       fprintf(stderr, "chmod(%s, 0700) failed: %s\n", path, strerror(errno));
	       fail++;
	  } else
	       chmod(path, 0666);

          if (chown(path, 1, 1) == -1) {
               fprintf(stderr, "chown(%s, 1, 1) failed: %s\n", path, strerror(errno));
               fail++;
          } else
               chown(path, 0, 0);
     }

     free(path);
     asprintf(&path, "%s/append-only.d/dir", dir);
     errno = 0;
     if (rmdir(path) != -1) {
	  fprintf(stderr, "rmdir(%s) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "rmdir(%s) did not set errno == EPERM\n", path);
	  fail++;
     }

     free(path);
     asprintf(&path, "%s/append-only.d", dir);
#ifdef TEST_UTIME
     errno = 0;
     if (utime(path, &tbuf) != -1) {
	  fprintf(stderr, "utime(%s, <epoch>) did not fail\n", path);
	  fail++;
     } else if (errno != EPERM) {
	  fprintf(stderr, "utime(%s, <epoch>) did not set errno == EPERM\n", path);
	  fail++;
     }

     errno = 0;
     if (utime(path, NULL) == -1) {
          fprintf(stderr, "utime(%s, NULL) failed\n", path);
          fail++;
     }
#endif /* TEST_UTIME */

     if (!getuid()) { /* these would fail if not root anyway */
	  errno = 0;
	  if (chmod(path, 7777) != -1) {
	       fprintf(stderr, "chmod(%s, 7777) did not fail\n", path);
	       fail++;
	       chmod(path, 0666);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chmod(%s, 7777) did not set errno == EPERM\n", path);
	       fail++;
	  }

	  errno = 0;
	  if (chown(path, 1, 1) != -1) {
	       fprintf(stderr, "chown(%s, 1, 1) did not fail\n", path);
	       fail++;
	       chown(path, 0, 0);
	  } else if (errno != EPERM) {
	       fprintf(stderr, "chown(%s, 1, 1) did not set errno to EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (del_acl(path) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "del_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "del_acl(%s) did not set errno == EPERM\n", path);
	       fail++;
	  }
	  errno = 0;
	  if (add_acl(path, acl_text) != 1) {
	       if (errno != EOPNOTSUPP) {
	       fprintf(stderr, "add_acl(%s) did not fail\n", path);
	       fail++;
	       }
	  } else if (errno != EPERM) {
	       fprintf(stderr, "add_acl(%s) did not set errno == EPERM\n", path);
               fail++;
          }

          if (setxattr(path, "trusted.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
               fail++;
          }
          if (setxattr(path, "trusted.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
               fprintf(stderr, "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "setxattr(%s, trusted.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
               fail++;
          }
          if (removexattr(path, "trusted.test") != -1) {
               fprintf(stderr, "removexattr(%s, trusted.test) did not fail\n", path);
               fail++;
          } else if (errno != EPERM && errno != EOPNOTSUPP) {
               fprintf(stderr,
                       "removexattr(%s, trusted.test) did not set errno == EPERM\n", path);
               fail++;
          }
     }

     if (setxattr(path, "user.test", "scribble", strlen("scribble"), XATTR_REPLACE) != -1) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.test, scribble, 8, XATTR_REPLACE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (setxattr(path, "user.scribble", "scribble", strlen("scribble"), XATTR_CREATE) != -1) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
          fprintf(stderr, "setxattr(%s, user.scribble, scribble, 8, XATTR_CREATE) did not set errno == EPERM\n", path);
          fail++;
     }
     if (removexattr(path, "user.test") != -1) {
          fprintf(stderr, "removexattr(%s, user.test) did not fail\n", path);
          fail++;
     } else if (errno != EPERM && errno != EOPNOTSUPP) {
	  perror("removexattr");
          fprintf(stderr,
                  "removexattr(%s, user.test) did not set errno == EPERM\n", path);
          fail++;
     }

     free(path);
     asprintf(&path, "%s/empty-append-only.d", dir);
     errno = 0;
     if (rmdir(path) != -1) {
          fprintf(stderr, "rmdir(%s) did not fail\n", path);
          fail++;
     } else if (errno != EPERM) {
          fprintf(stderr, "rmdir(%s) did not set errno == EPERM\n", path);
          fail++;
     }

     free(path);
     return fail;
}

static int check_test_area(const char *dir)
{
     char *path;
     struct stat st;

     asprintf(&path, "%s/", dir);
     if (stat(path, &st) == -1) {
	  fprintf(stderr, "%s: %s: %s\n", __progname, path, strerror(errno));
	  return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) || st.st_uid) {
	  fprintf(stderr, "%s: %s needs to be rwx for for all, and owner uid should be 0\n",
		  __progname, path);
	  return 1;
     }

     free(path);
     asprintf(&path, "%s/immutable.f", dir);
     if (stat(path, &st) == -1) {
	  perror(path);
	  return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) || st.st_uid || !S_ISREG(st.st_mode)) {
	  fprintf(stderr, "%s: %s needs to be a regular file, rw for all, and owner uid should be 0\n",
		  __progname, path);
	  return 1;
     }

     free(path);
     asprintf(&path, "%s/append-only.f", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) || st.st_uid || !S_ISREG(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a regular file, rw for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/immutable.d", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) ||
	 st.st_uid || !S_ISDIR(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a directory, rwx for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/append-only.d", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) ||
	 st.st_uid || !S_ISDIR(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a directory, rwx for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/immutable.d/file", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) || st.st_uid || !S_ISREG(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a regular file, rw for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/append-only.d/file", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) || st.st_uid || !S_ISREG(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a regular file, rw for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/immutable.d/dir", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) ||
         st.st_uid || !S_ISDIR(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a directory, rwx for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }

     free(path);
     asprintf(&path, "%s/append-only.d/dir", dir);
     if (stat(path, &st) == -1) {
          perror(path);
          return 1;
     }
     if (!(st.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)) ||
         st.st_uid || !S_ISDIR(st.st_mode)) {
          fprintf(stderr, "%s: %s needs to be a directory, rwx for all, and owner uid should be 0\n",
                  __progname, path);
          return 1;
     }
     return 0;
}

static int create_test_area(const char *dir)
{
     int fd;
     char *path;
     static const char *acl_u_text = "u::rw-,g::rw-,o::rw-,u:nobody:rw-,m::rw-";
     static const char *acl_u_text_d = "u::rwx,g::rwx,o::rwx,u:nobody:rwx,m::rwx";
     struct stat st;
     static const char *immutable = "This is an immutable file.\nIts contents cannot be altered.\n";
     static const char *append_only = "This is an append-only file.\nIts contents cannot be altered.\n"
	  "Data can only be appended.\n---\n";

     if (getuid()) {
	  fprintf(stderr, "%s: you are not root, go away.\n", __progname);
	  return 1;
     }

     if (stat(dir, &st) == 0) {
	  fprintf(stderr, "%s: Test area directory %s must not exist for test area creation.\n",
		  __progname, dir);
	  return 1;
     }

     umask(0000);
     if (mkdir(dir, 0777) != 0) {
	  fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, dir, strerror(errno));
	  return 1;
     }

     asprintf(&path, "%s/immutable.d", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/empty-immutable.d", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/append-only.d", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/empty-append-only.d", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/immutable.d/dir", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/append-only.d/dir", dir);
     if (mkdir(path, 0777) != 0) {
          fprintf(stderr, "%s: error creating directory %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     free(path);

     asprintf(&path, "%s/append-only.d/file", dir);
     if ((fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0666)) == -1) {
	  fprintf(stderr, "%s: error creating file %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/immutable.d/file", dir);
     if ((fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0666)) == -1) {
          fprintf(stderr, "%s: error creating file %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/immutable.f", dir);
     if ((fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0666)) == -1) {
          fprintf(stderr, "%s: error creating file %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (write(fd, immutable, strlen(immutable)) != strlen(immutable)) {
	  fprintf(stderr, "%s: error writing file %s: %s\n", __progname, path, strerror(errno));
	  return 1;
     }
     if (fadd_acl(fd, acl_u_text)) {
	  perror("acl");
	  return 1;
     }
     if (fsetxattr(fd, "trusted.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetxattr(fd, "user.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetflag(path, fd, 1, 1)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/append-only.f", dir);
     if ((fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0666)) == -1) {
          fprintf(stderr, "%s: error creating file %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (write(fd, append_only, strlen(append_only)) != strlen(append_only)) {
          fprintf(stderr, "%s: error writing file %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (fadd_acl(fd, acl_u_text)) {
          perror("acl");
          return 1;
     }
     if (fsetxattr(fd, "trusted.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetxattr(fd, "user.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetflag(path, fd, 1, 0)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/immutable.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (fadd_acl(fd, acl_u_text_d)) {
          perror("acl");
          return 1;
     }
     if (fsetxattr(fd, "trusted.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetxattr(fd, "user.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetflag(path, fd, 1, 1)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/empty-immutable.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (fsetflag(path, fd, 1, 1)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/append-only.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (fadd_acl(fd, acl_u_text_d)) {
          perror("acl");
          return 1;
     }
     if (fsetxattr(fd, "trusted.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetxattr(fd, "user.test", "readonly", strlen("readonly"), XATTR_CREATE) != 0) {
	  if (errno != EOPNOTSUPP) {
	       perror("setxattr");
	       return 1;
	  }
     }
     if (fsetflag(path, fd, 1, 0)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);

     asprintf(&path, "%s/empty-append-only.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
          return 1;
     }
     if (fsetflag(path, fd, 1, 0)) {
          perror("fsetflag");
          close(fd);
          return 1;
     }
     close(fd);
     free(path);
     return 0;
}

static int remove_test_area(const char *dir)
{
     int fd;
     int ret;
     int err = 0;
     char *path;
     pid_t pid;
     struct stat st;

     if (getuid()) {
	  fprintf(stderr, "%s: you are not root, go away.\n", __progname);
	  return 1;
     }

     if (stat(dir, &st) == -1) {
	  fprintf(stderr, "%s: cannot remove test area %s: %s\n", __progname, dir, strerror(errno));
	  return 1;
     }

     asprintf(&path, "%s/immutable.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 1))
	       perror("fsetflag");
	  close(fd);
     }
     free(path);

     asprintf(&path, "%s/empty-immutable.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 1))
	       perror("fsetflag");

	  close(fd);
     }
     free(path);

     asprintf(&path, "%s/append-only.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 0))
	       perror("fsetflag");
	  close(fd);
     }
     free(path);

     asprintf(&path, "%s/empty-append-only.d", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 0))
	       perror("fsetflag");
	  close(fd);
     }
     free(path);

     asprintf(&path, "%s/append-only.f", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 0))
	       perror("fsetflag");

	  close(fd);
     }
     free(path);

     asprintf(&path, "%s/immutable.f", dir);
     if ((fd = open(path, O_RDONLY)) == -1) {
          fprintf(stderr, "%s: error opening %s: %s\n", __progname, path, strerror(errno));
	  err = 1;
     } else {
	  if (fsetflag(path, fd, 0, 1))
	       perror("fsetflag");
	  close(fd);
     }
     free(path);

     if (err) {
	  fprintf(stderr, "%s: Warning, expected parts of the test area missing, not removing.\n", __progname);
	  return 1;
     }

     pid = fork();
     if (!pid) {
	  execl("/bin/rm", "rm", "-rf", dir, NULL);
	  return 1;
     } else if (pid == -1) {
	  perror("fork failed");
	  return 1;
     }
     wait(&ret);

     return WEXITSTATUS(ret);
}

int main(int argc, char **argv)
{
     int ret;
     int failed = 0;

/* this arg parsing is gross, but who cares, its a test program */

     if (argc < 2) {
	  fprintf(stderr, "usage: t_immutable [-C|-c|-r] test_area_dir\n");
	  return 1;
     }

     if (!strcmp(argv[1], "-c")) {
	  if (argc == 3) {
	       if ((ret = create_test_area(argv[argc-1])))
		    return ret;
	  } else {
	       fprintf(stderr, "usage: t_immutable -c test_area_dir\n");
	       return 1;
	  }
     } else if (!strcmp(argv[1], "-C")) {
          if (argc == 3) {
               return create_test_area(argv[argc-1]);
          } else {
               fprintf(stderr, "usage: t_immutable -C test_area_dir\n");
               return 1;
          }
     } else if (!strcmp(argv[1], "-r")) {
	  if (argc == 3)
	       return remove_test_area(argv[argc-1]);
	  else {
	       fprintf(stderr, "usage: t_immutable -r test_area_dir\n");
	       return 1;
	  }
     } else if (argc != 2) {
	  fprintf(stderr, "usage: t_immutable [-c|-r] test_area_dir\n");
	  return 1;
     }

     umask(0000);

     if (check_test_area(argv[argc-1]))
	  return 1;

     printf("testing immutable...");
     if ((ret = test_immutable(argv[argc-1])) != 0) {
	  printf("FAILED! (%d tests failed)\n", ret);
	  failed = 1;
     } else
	  puts("PASS.");

     printf("testing append-only...");
     if ((ret = test_append(argv[argc-1])) != 0) {
          printf("FAILED! (%d tests failed)\n", ret);
          failed = 1;
     } else
	  puts("PASS.");

     if (!getuid() && !failed) {
	  if (setgroups(0, NULL) != 0)
	       perror("setgroups");
	  if (setgid(65534) != 0)
	       perror("setgid");
	  if (setuid(65534) != 0)
	       perror("setuid");
	  printf("testing immutable as non-root...");
	  if ((ret = test_immutable(argv[argc-1])) != 0) {
	       printf("FAILED! (%d tests failed)\n", ret);
	       failed = 1;
	  } else
	       puts("PASS.");

	  printf("testing append-only as non-root...");
	  if ((ret = test_append(argv[argc-1])) != 0) {
	       printf("FAILED! (%d tests failed)\n", ret);
	       failed = 1;
	  } else
	       puts("PASS.");
     }

     return failed;
}
