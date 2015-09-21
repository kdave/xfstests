/*
 * Test ctime and mtime are updated on truncate(2) and ftruncate(2)
 *
 * Copyright (c) 2013 Red Hat, Inc.  All Rights Reserved.
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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_MSG "this is a test string"

int do_test(const char *file, int is_ftrunc)
{
	int ret;
	int fd;
	struct stat statbuf1;
	struct stat statbuf2;

	ret = 0;
	fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		perror("open(2) failed");
		exit(EXIT_FAILURE);
	}

	ret = write(fd, TEST_MSG, sizeof(TEST_MSG));
	if (ret == -1) {
		perror("write(2) failed");
		exit(EXIT_FAILURE);
	}

	/* Get timestamps before [f]truncate(2) */
	ret = fstat(fd, &statbuf1);
	if (ret == -1) {
		perror("fstat(2) failed");
		exit(EXIT_FAILURE);
	}

	/* Test [f]truncate(2) down */
	sleep(1);
	if (is_ftrunc) {
		ret = ftruncate(fd, 0);
		if (ret == -1) {
			perror("ftruncate(2) down failed");
			exit(EXIT_FAILURE);
		}
	} else {
		ret = truncate(file, 0);
		if (ret == -1) {
			perror("truncate(2) down failed");
			exit(EXIT_FAILURE);
		}
	}

	/* Get timestamps after [f]truncate(2) */
	ret = fstat(fd, &statbuf2);
	if (ret == -1) {
		perror("fstat(2) failed");
		exit(EXIT_FAILURE);
	}

	/* Check whether timestamps got updated on [f]truncate(2) down */
	if (statbuf1.st_ctime == statbuf2.st_ctime) {
		fprintf(stderr, "ctime not updated after %s\n",
			is_ftrunc ? "ftruncate" : "truncate" " down");
		ret++;
	}
	if (statbuf1.st_mtime == statbuf2.st_mtime) {
		fprintf(stderr, "mtime not updated after %s\n",
			is_ftrunc ? "ftruncate" : "truncate" " down");
		ret++;
	}

	/* Test [f]truncate(2) up */
	sleep(1);
	if (is_ftrunc) {
		ret = ftruncate(fd, 123);
		if (ret == -1) {
			perror("ftruncate(2) up failed");
			exit(EXIT_FAILURE);
		}
	} else {
		ret = truncate(file, 123);
		if (ret == -1) {
			perror("truncate(2) up failed");
			exit(EXIT_FAILURE);
		}
	}
	ret = fstat(fd, &statbuf1);
	if (ret == -1) {
		perror("fstat(2) failed");
		exit(EXIT_FAILURE);
	}
	/* Check whether timestamps got updated on [f]truncate(2) up */
	if (statbuf1.st_ctime == statbuf2.st_ctime) {
		fprintf(stderr, "ctime not updated after %s\n",
			is_ftrunc ? "ftruncate" : "truncate" " up");
		ret++;
	}
	if (statbuf1.st_mtime == statbuf2.st_mtime) {
		fprintf(stderr, "mtime not updated after %s\n",
			is_ftrunc ? "ftruncate" : "truncate" " up");
		ret++;
	}

	close(fd);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret;
	char *testfile;

	ret = 0;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	testfile = argv[1];

	ret = do_test(testfile, 0);
	ret += do_test(testfile, 1);

	exit(ret);
}
