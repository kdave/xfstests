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
 
/*
 *
 * usemem: allocate and lock a chunk of memory effectively removing
 *         it from the usable physical memory range
 *
 *                                                  - dxm 04/10/00
 */

#include <stdio.h>
#include <malloc.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

void
usage(char *argv0)
{
    fprintf(stderr,"Usage: %s <mb>\n", argv0);
    exit(1);
}

static void
signalled(int sig)
{
    printf("*** signal\n");
}

int
main(int argc, char *argv[])
{
    int mb;
    char *buf;
    
    if (argc!=2) usage(argv[0]);
    mb=atoi(argv[1]);
    if (mb<=0) usage(argv[0]);
    
    buf=malloc(mb*1024*1024);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    if (mlock(buf,mb*1024*1024)) {
        perror("mlock");
        exit(1);
    }
    
    printf("%s: %d mb locked - interrupt to release\n", argv[0], mb);
    signal(SIGINT, signalled);
    pause();
    printf("%s: %d mb unlocked\n", argv[0], mb);
    
    return 0;
}


