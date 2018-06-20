// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <xfs/xfs.h>
struct xfs_flock64 f;

int main(int argc, char **argv)
{
   int fd, i;

   int64_t x[131072];

   for (i = 0; i < 131072; i++) {
      x[i] = -1;
   }
   fd = open(argv[1], O_RDWR | O_CREAT, 0644);

#ifdef WRITE
   f.l_whence = 0;
   f.l_start = 0;
   f.l_len = 1048576;
   xfsctl (argv[1], fd, XFS_IOC_RESVSP, &f);
   for (i = 0; i < 131072; i++) {
      x[i] = i;
   }
   write(fd, &x, 1048576);
#endif

#ifdef READ
   read(fd, &x, 1048576);
   for (i = 0; i < 131072; i++) {
      if (x[i] != i) {
         printf("error: %d %d %lld\n", i ,8 * i, (long long)x[i]);
	 exit(1);
      }
   }
#endif

   close(fd);
   exit(0);
}

