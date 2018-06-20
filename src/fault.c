// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
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
