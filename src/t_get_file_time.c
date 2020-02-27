// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2009 FUJITSU LIMITED
 * Author: Li Zefan <lizf@cn.fujitsu.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*
 * Usage: t_get_file_time <filename> <atime|mtime|ctime> <sec|nsec>
 */
int main(int argc, char *argv[])
{
       time_t t;
       struct stat st;

       if (argc != 4) {
               fprintf(stderr, "Wrong argument num!\n");
               return 1;
       }

       if (stat(argv[1], &st) != 0) {
               perror("stat failed");
               return 1;
       }

       if (strcmp(argv[3], "sec") == 0) {
               if (strcmp(argv[2], "atime") == 0)
                       t = st.st_atime;
               else if (strcmp(argv[2], "mtime") == 0)
                       t = st.st_mtime;
               else
                       t = st.st_ctime;
       } else if (strcmp(argv[3], "nsec") == 0) {
               if (strcmp(argv[2], "atime") == 0)
                       t = st.st_atim.tv_nsec;
               else if (strcmp(argv[2], "mtime") == 0)
                       t = st.st_mtim.tv_nsec;
               else
                       t = st.st_ctim.tv_nsec;
       } else {
               fprintf(stderr, "Wrong argument: %s\n", argv[3]);
               return 1;
       }

       printf("%lu\n", t);

       return 0;
}
