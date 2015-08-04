/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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

