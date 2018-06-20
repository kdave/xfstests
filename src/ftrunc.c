// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(argc, argv)
int     argc;
char    **argv;

{
char *filename="testfile";
int c;
int fd, err;

if(argc != 3)
        {        printf("Usage: cxfs_ftrunc -f filename\n");
                exit(1);
        }

while((c=getopt(argc,argv,"f:"))!=EOF) {
                switch (c) {
                case 'f':
                        filename = optarg;
                        break;
                default:
                        fprintf(stderr,"Usage: ftrunc -f filename\n");
                        exit(1);
                }
        }




          /* open RDWR but with read-only perms */
          fd = open(filename, O_CREAT|O_RDWR, 0400);
          if (fd < 0) {
                perror("open failed");
                return 1;
                }

          err = ftruncate(fd, 1000);
          if (err < 0) {
              perror("ftruncate failed");
              return 1;
           }

          err = unlink(filename);
          if (err < 0) {
                perror("unlink failed");
                return 1;
          }     

          return 0;

}
