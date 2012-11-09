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
#include <attr/attributes.h>

#define MAX_EA_NAME 30

/*
 * multi_open_unlink path_prefix num_files sleep_time
 * e.g.
 *   $ multi_open_unlink file 100 60
 *   Creates 100 files: file.1, file.2, ..., file.100
 *   unlinks them all but doesn't close them all until after 60 seconds.
 */

void
usage(char *prog)
{
	fprintf(stderr, "Usage: %s [-e num-eas] [-f path_prefix] [-n num_files] [-s sleep_time] [-v ea-valuesize] \n", prog);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *given_path = "file";
	char path[PATH_MAX];
	char *prog = argv[0];
	int sleep_time = 60;
	int num_files = 100;
	int num_eas = 0;
	int value_size = ATTR_MAX_VALUELEN;
	int fd = -1;
	int i,j,c;

	while ((c = getopt(argc, argv, "e:f:n:s:v:")) != EOF) {
		switch (c) {
			case 'e':   /* create eas */
				num_eas = atoi(optarg);
				break;
			case 'f':   /* file prefix */
				given_path = optarg;
				break;
			case 'n':   /* number of files */
				num_files = atoi(optarg);
				break;
			case 's':   /* sleep time */
				sleep_time = atoi(optarg);
				break;
			case 'v':  /* value size on eas */
				value_size = atoi(optarg);
				break;
			case '?':
				usage(prog);
		}
	}

        /* create and unlink a bunch of files */
	for (i = 0; i < num_files; i++) {
		sprintf(path, "%s.%d", given_path, i+1);

		/* if file already exists then error out */
		if (access(path, F_OK) == 0) {
			fprintf(stderr, "%s: file \"%s\" already exists\n", prog, path);
			return 1;
		}

		fd = open(path, O_RDWR|O_CREAT|O_EXCL, 0666);
		if (fd == -1) {
			fprintf(stderr, "%s: failed to create \"%s\": %s\n", prog, path, strerror(errno));
			return 1;
		}

		/* set the EAs */
		for (j = 0; j < num_eas; j++) {
			int sts;
			char *attrvalue;
			char attrname[MAX_EA_NAME];
			int flags = 0;

			snprintf(attrname, MAX_EA_NAME, "user.name.%d", j);

			attrvalue = calloc(value_size, 1);
			if (attrvalue == NULL) {
				fprintf(stderr, "%s: failed to create EA value of size %d on path \"%s\": %s\n",
					prog, value_size, path, strerror(errno));
				return 1;
			}

			sts = attr_set(path, attrname, attrvalue, value_size, flags);
			if (sts == -1) {
				fprintf(stderr, "%s: failed to create EA \"%s\" of size %d on path \"%s\": %s\n",
					prog, attrname, value_size, path, strerror(errno));
				return 1;
			}
		}

		if (unlink(path) == -1) {
			fprintf(stderr, "%s: failed to unlink \"%s\": %s\n",
				prog, path, strerror(errno));
			return 1;
		}
	}

	sleep(sleep_time);

	return 0;
}
