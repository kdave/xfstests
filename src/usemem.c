// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
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


