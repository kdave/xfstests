/*
 * Generates files or directories with hash collisions on a XFS filesystem
 * Copyright (C) 2014 Hannes Frederic Sowa
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

static enum {
	ILLEGAL,
	DIRECTORY,
	FILENAME,
} touch_mode = ILLEGAL;

static bool one_hash = false;

static uint32_t rol32(uint32_t word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

static uint32_t xfs_da_hashname(const uint8_t *name, int namelen)
{
	uint32_t hash;

	for (hash = 0; namelen >= 4; namelen -= 4, name += 4)
		hash = (name[0] << 21) ^ (name[1] << 14) ^ (name[2] << 7) ^
		       (name[3] << 0) ^ rol32(hash, 7 * 4);

	if (namelen) {
		fprintf(stderr,
			"internal error: "
			"misbalanced input buffer to xfs_da_hashname - "
			"overlapping %d bytes\n", namelen);
		exit(1);
	}

	return hash;
}

static uint8_t gen_rand(void)
{
	uint8_t r;
	while (!(r = rand()));
	return r;
}

static uint8_t buffer[252+1] = {0};

static void gen_name(void)
{
	int idx;
	uint32_t hash, last;

again:
	for (idx = 0; idx < 252-4; idx+=4) {
		buffer[idx + 0] = gen_rand();
		buffer[idx + 1] = gen_rand();
		buffer[idx + 2] = gen_rand();
		buffer[idx + 3] = gen_rand();
	}

	hash = rol32(xfs_da_hashname(buffer, 248), 7 * 4);
	last = hash ^ ~0U;

	if (last == 0)
		goto again;

	buffer[idx + 3] = last & 0x7fU;
	buffer[idx + 2] = (last >> 7)  & 0x7fU;
	buffer[idx + 1] = (last >> 14) & 0x7fU;
	buffer[idx + 0] = ((last >> 21) & 0xffU);

	if (memchr(buffer, '.', sizeof(buffer)) ||
	    memchr(buffer, '/', sizeof(buffer)))
		goto again;

	if (one_hash) {
		/* very poor - can be improved later */
		static bool done = false;
		static uint32_t filter;

		if (!done) {
			filter = xfs_da_hashname(buffer, 252);
			done = true;
			return;
		}

		if (filter != xfs_da_hashname(buffer, 252))
			goto again;
	}
}

static int touch(const char *buffer)
{
	if (touch_mode == DIRECTORY) {
		if (mkdir(buffer, S_IRWXU)) {
			/* ignore if directory is already present */
			if (errno == EEXIST)
				return 0;
			perror("mkdir with random directory name");
			return 1;
		}
	} else if (touch_mode == FILENAME) {
		int fd = creat(buffer, S_IRWXU);
		if (fd == -1) {
			/* ignore duplicate files */
			if (errno == EEXIST)
				return 0;
			perror("creat with random directory name");
			return 1;
		}
		if (close(fd)) {
			perror("close is leaking a file descriptor");
			return 1;
		}
		return 0;
	}
	return 0;
}

static void do_seed(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday");
		exit(1);
	}
	srand(tv.tv_sec ^ tv.tv_usec ^ getpid());
}

static void usage_and_exit(const char *pname)
{
	fprintf(stderr, "usage: %s [-d] [-f] [-n num] [-s] directory\n"
		        "\t-f\tcreate files (the default)\n"
			"\t-d\tcreate directories\n"
			"\t-n num\tcreate num directories or files (default 200000)\n"
			"\t-s\tonly generate one hash\n"
			"\tdirectory\tthe directory to chdir() to\n",
		pname);
	exit(1);
}

int main(int argc, char **argv)
{
	const char allopts[] = "hsdfn:";
	int c, orig_cycles, errors = 0, cycles = 200000;

	while ((c = getopt(argc, argv, allopts)) != -1) {
		switch (c) {
		case 'd':
			if (touch_mode != ILLEGAL)
				usage_and_exit(argv[0]);
			touch_mode = DIRECTORY;
			break;
		case 'f':
			if (touch_mode != ILLEGAL)
				usage_and_exit(argv[0]);
			touch_mode = FILENAME;
			break;
		case 'n':
			errno = 0;
			if (sscanf(optarg, "%d", &cycles) != 1 ||
			    errno == ERANGE) {
				fputs("could not parse number of iterations", stderr);
				exit(1);
			}
			break;
		case 's':
			one_hash = true;
			break;
		default:
			usage_and_exit(argv[0]);
			break;
		}
	}

	if (argc <= optind || touch_mode == ILLEGAL)
		usage_and_exit(argv[0]);

	if (chdir(argv[optind])) {
		perror("chdir");
		exit(1);
	}

	orig_cycles = cycles;

	do_seed();

	while (cycles--) {
		gen_name();
		errors += touch((char *)buffer);
	}

	if (errors)
		fprintf(stderr, "creating %d %s caused %d errors\n",
			orig_cycles, touch_mode == FILENAME ? "files" : "directories",
			errors);

	return 0;
}
