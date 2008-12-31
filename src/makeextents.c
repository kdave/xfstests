/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.
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

/*
 * Write a bunch of holes to create a bunch of extents.
 */
 
#include "global.h"

char *progname;
__uint64_t num_holes = 1000;
__uint64_t curr_holes;
int verbose_opt = 0;
char *filename;
int status_num = 100;
int wsync;
int preserve;
unsigned int blocksize;
__uint64_t fileoffset;

#define JUMP_SIZE (128 * 1024)
#define NUMHOLES_TO_SIZE(i) (i * JUMP_SIZE)
#define SIZE_TO_NUMHOLES(s) (s / JUMP_SIZE)

void
usage(void)
{
	fprintf(stderr, "%s [-b blocksize] [-n num-holes] [-s status-num]"
			" [-o start-offset] [-vwp] file\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	int fd;
	int oflags;
	__uint64_t i;
	__uint64_t offset;
	int blocksize = 512;
	unsigned char *buffer = NULL;
	struct stat stat;

	progname = argv[0];

	while ((c = getopt(argc, argv, "b:n:o:ps:vw")) != -1) {
		switch (c) {
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'n':
			num_holes = atoll(optarg);
			break;
		case 'v':
			verbose_opt = 1;
			break;
		case 'w':
			wsync = 1;
			break;
		case 'p':
			preserve = 1;
			break;
		case 's':
			status_num = atoi(optarg);
			break;
		case 'o':
			fileoffset = strtoull(optarg, NULL, 16);
			break;
		case '?':
			usage();
		}
	}
	if (optind == argc-1)
		filename = argv[optind];
	else
		usage();

	buffer = malloc(blocksize);
	if (buffer == NULL) {
	    fprintf(stderr, "%s: blocksize to big to allocate buffer\n", progname);
	    return 1;
	}

        oflags = O_RDWR | O_CREAT;
	oflags |=   (preserve ? 0 : O_TRUNC) |
		    (wsync ? O_SYNC : 0);
        
	if ((fd = open(filename, oflags, 0666)) < 0) {
		perror("open");
		return 1;
	}

	if (fstat(fd, &stat) < 0) {
		perror("stat");
		return 1;
	}
	if (preserve) {
		curr_holes = SIZE_TO_NUMHOLES(stat.st_size);
		if (num_holes < curr_holes) {
			/* we need to truncate back */
			if (ftruncate(fd, NUMHOLES_TO_SIZE(num_holes)) < 0) {
				perror("ftruncate");
				return 1;
			}
			if (verbose_opt) {
				printf("truncating back to %llu\n",
				       (unsigned long long)
					NUMHOLES_TO_SIZE(num_holes));
			}
			return 0;
		}
	}
	else {
		curr_holes = 0;
	}
	if (curr_holes != 0 && verbose_opt) {
		printf("creating %llu more holes\n",
			(unsigned long long)num_holes - curr_holes);
	}
		
	/* create holes by seeking and writing */
	for (i = curr_holes; i < num_holes; i++) {

		offset = NUMHOLES_TO_SIZE(i) + fileoffset;

		if (lseek64(fd, offset, SEEK_SET) < 0) {
			perror("lseek");
			return 1;
		}

		if (write(fd, buffer, blocksize) < blocksize) {
			perror("write");
			return 1;
		}

		if (verbose_opt && ((i+1) % status_num == 0)) {
			printf("seeked and wrote %llu times\n",
				(unsigned long long)i + 1);
		}
	}

	close(fd);

	return 0;
}
