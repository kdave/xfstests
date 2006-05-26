/*
 * Copyright (c) 2006 Silicon Graphics, Inc.
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * multi_open_unlink path_prefix num_files sleep_time
 * e.g.  
 *   $ multi_open_unlink file 100 60
 *   Creates 100 files: file.1, file.2, ..., file.100
 *   unlinks them all but doesn't close them all until after 60 seconds. 
 */

int
main(int argc, char *argv[])
{
	char *given_path;
	char path[PATH_MAX];
	char *prog = argv[0];
	int sleep_time;
	int num_files;
	int fd = -1;
	int i;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s path_prefix num_files sleep_time\n", prog);
		return 1;
	}

	given_path = argv[1];
	num_files = atoi(argv[2]);
	sleep_time = atoi(argv[3]);

        /* create and unlink a bunch of files */
	for (i = 0; i < num_files; i++) {
		sprintf(path, "%s.%d", given_path, i+1);

		/* if file already exists then error out */
		if (access(path, F_OK) == 0) {
			fprintf(stderr, "%s: file \"%s\" already exists\n", prog, path);
			return 1;
		}

		fd = open(path, O_RDWR|O_CREAT|O_EXCL);
		if (fd == -1) {
			fprintf(stderr, "%s: failed to create \"%s\": %s\n", prog, path, strerror(errno));
			return 1;
		}

		if (unlink(path) == -1) {
			fprintf(stderr, "%s: failed to unlink \"%s\": %s\n", prog, path, strerror(errno));
			return 1;
		}
	}

	sleep(sleep_time);

	return 0;
}
