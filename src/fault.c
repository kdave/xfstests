/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <libxfs.h>
#include <sys/ioctl.h>

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
    
    printf("--- ioctl with bad output address\n");
    expect_error(ioctl(fsfd, XFS_IOC_FSCOUNTS, NULL), EFAULT);
    printf("--- ioctl with bad input address\n");
    expect_error(ioctl(fsfd, XFS_IOC_SET_RESBLKS, NULL), EFAULT);
    
    close(fsfd);
    
    return 0;
}
