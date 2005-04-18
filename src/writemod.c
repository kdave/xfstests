/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * tests out if access checking is done on write path
 * 1. opens with write perms
 * 2. fchmod to turn off write perms
 * 3. writes to file
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

int 
main(int argc, char* argv[])
{
    char *path;
    int fd;
    char *buf = "hi there";
    ssize_t x;
    int sts;

    if (argc != 2) {
	fprintf(stderr, "%s: requires path argument\n", argv[0]);
        return 1;
    }

    path = argv[1];

    printf("open for write \"%s\" with 777\n", path);
    fd = open(path, O_RDWR, 0777);
    if (fd == -1) {
	perror("open");
        return 1;
    }
    printf("remove perms on file\n");
    sts = fchmod(fd, 0);
    if (sts == -1) {
	perror("fchmod");
        return 1;
    }
    printf("write to the file\n");
    x = write(fd, buf, strlen(buf)+1); 
    if (x == -1) {
	perror("write");
        return 1;
    }
    return 0;
}
