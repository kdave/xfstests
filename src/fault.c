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

#include "global.h"

void expect_error(int r, int err)
{
    if (r<0) {
        if (errno == err) {
            printf("   --- got error %d as expected\n", err);   
        } else {
            perror("   !!! unexpected error");
        }
    } else {
        printf("   !!! success instead of error %d as expected\n", err);
    }
}

int
main(int argc, char **argv)
{
    int                 fsfd;
    
    if (argc != 2) {
        fprintf(stderr,"usage: %s <filesystem>\n",
                argv[0]);
        exit(0);
    }

    fsfd = open(argv[1], O_RDONLY);

    if (fsfd < 0) {
	perror("open");
	exit(1);
    }
    
    printf("--- xfsctl with bad output address\n");
#ifdef XFS_IOC_FSCOUNTS
    expect_error(xfsctl(argv[1], fsfd, XFS_IOC_FSCOUNTS, NULL), EFAULT);
#else
#ifdef XFS_FS_COUNTS
    expect_error(syssgi(SGI_XFS_FSOPERATIONS, fsfd, XFS_FS_COUNTS, NULL, NULL), EFAULT);
#else
bozo!
#endif
#endif

    printf("--- xfsctl with bad input address\n");
#ifdef XFS_IOC_SET_RESBLKS
    expect_error(xfsctl(argv[1], fsfd, XFS_IOC_SET_RESBLKS, NULL), EFAULT);
#else
#ifdef XFS_SET_RESBLKS
    expect_error(syssgi(SGI_XFS_FSOPERATIONS, fsfd, XFS_SET_RESBLKS, NULL, NULL), EFAULT);
#else
bozo!
#endif
#endif
    
    close(fsfd);
    
    return 0;
}
