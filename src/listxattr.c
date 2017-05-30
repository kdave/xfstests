/*
 * Copyright (c) 2016 Red Hat, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/xattr.h>

int main(int argc, char **argv)
{
        int ret;
        size_t bufsize = 0;
        char *buf = NULL;

        if (argc < 2) {
                fprintf(stderr, "usage: %s <testfile> [bufsize]\n", argv[0]);
                return 1;
        };

        if (argc > 2) {
                bufsize = strtoul(argv[2], NULL, 10);
                if (bufsize == -1) {
                        perror("buffsize");
                        return 1;
                }
        }

        if (bufsize == 0) {
                bufsize = listxattr(argv[1], NULL, 0);
                if (bufsize == -1) {
                        perror("listxattr");
                        return 1;
                }
        }

        buf = malloc(bufsize);
        if (buf == NULL) {
                perror("buf alloc");
                return 1;
        }

        ret = listxattr(argv[1], buf, bufsize);
        if (ret < 0) {
                perror("listxattr");
        } else {
                char *l = buf;
                while (l < (buf + ret)) {
                        printf("xattr: %s\n", l);
                        l = strchr(l, '\0') + 1;
                }
        }

        free(buf);

        return 0;
}
