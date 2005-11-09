/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
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
 
#include "global.h"

long filesize;
int blocksize;
int count;
int verbose;
off64_t fileoffset;

void usage(char *progname);
void writeblk(int fd);
void truncfile(int fd);

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-l filesize] [-b blocksize] [-c count] [-s seed] [-o fileoffset (hex)] [-v] filename\n",
			progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int seed, i, ch, fd;
	char *filename = NULL;

	filesize = 256*1024*1024;
	blocksize = 512;
	count = filesize/blocksize;
	verbose = 0;
	fileoffset = 0;
	seed = time(NULL);
	while ((ch = getopt(argc, argv, "b:l:s:c:o:v")) != EOF) {
		switch(ch) {
		case 'b':	blocksize  = atoi(optarg);	break;
		case 'l':	filesize   = atol(optarg);	break;
		case 's':	seed       = atoi(optarg);	break;
		case 'c':	count      = atoi(optarg);	break;
		case 'o':	fileoffset = strtoll(optarg, NULL, 16); break;
		case 'v':	verbose++;			break;
		default:	usage(argv[0]);			break;
		}
	}
	if (optind == argc-1)
		filename = argv[optind];
	else
		usage(argv[0]);
	printf("Seed = %d (use \"-s %d\" to re-execute this test)\n", seed, seed);
	srandom(seed);

	/*
	 * Open the file, write rand block in random places, truncate the file,
	 * repeat ad-nauseum, then close the file.
	 */
	if ((fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		perror("open");
		return 1;
	}
	for (i = 0; i < count; i++) {
		writeblk(fd);
		truncfile(fd);
		if (verbose && ((i % 100) == 0)) {
			printf(".");
			fflush(stdout);
		}
	}
	if (close(fd) < 0) {
		perror("close");
		return 1;
	}
	return 0;
}

void
writeblk(int fd)
{
	off_t offset;
	static char *buffer = NULL;

	if ((buffer == NULL) && ((buffer = calloc(1, blocksize)) == NULL)) {
		perror("malloc");
		exit(1);
	}

	offset = random() % filesize;
	if (lseek64(fd, (off64_t)(fileoffset + offset), SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}
	*(long *)buffer = *(long *)(buffer+256) = (long)offset;
	if (write(fd, buffer, blocksize) < blocksize) {
		perror("write");
		exit(1);
	}
	if (verbose > 1)
		printf("writing   data at offset=%llx\n",
		       (unsigned long long)(fileoffset + offset));
}

void
truncfile(int fd)
{
	off_t offset;

	offset = random() % filesize;
	if (ftruncate64(fd, (off64_t)(fileoffset + offset)) < 0) {
		perror("truncate");
		exit(1);
	}
	if (verbose > 1)
		printf("truncated file to offset %llx\n",
		       (unsigned long long)(fileoffset + offset));
}
