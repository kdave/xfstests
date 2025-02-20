// SPDX-License-Identifier: GPL-2.0+
/*
 * dio-writeback-race -- test direct IO writes with the contents of
 * the buffer changed during writeback.
 *
 * Copyright (C) 2025 SUSE Linux Products GmbH.
 */

/*
 * dio-writeback-race
 *
 * Compile:
 *
 *   gcc -Wall -pthread -o dio-writeback-race dio-writeback-race.c
 *
 * Make sure filesystems with explicit data checksum can handle direct IO
 * writes correctly, so that even the contents of the direct IO buffer changes
 * during writeback, the fs should still work fine without data checksum mismatch.
 * (Normally by falling back to buffer IO to keep the checksum and contents
 *  consistent)
 *
 * Usage:
 *
 *   dio-writeback-race [-b <buffersize>] [-s <filesize>] <filename>
 *
 * Where:
 *
 *   <filename> is the name of the test file to create, if the file exists
 *   it will be overwritten.
 *
 *   <buffersize> is the buffer size for direct IO. Users are responsible to
 *   probe the correct direct IO size and alignment requirement.
 *   If not specified, the default value will be 4096.
 *
 *   <filesize> is the total size of the test file. If not aligned to <blocksize>
 *   the result file size will be rounded up to <blocksize>.
 *   If not specified, the default value will be 64MiB.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static int fd = -1;
static void *buf = NULL;
static unsigned long long filesize = 64 * 1024 * 1024;
static int blocksize = 4096;

const int orig_pattern = 0xff;
const int modify_pattern = 0x00;

static void *do_io(void *arg)
{
	ssize_t ret;

	ret = write(fd, buf, blocksize);
	pthread_exit((void *)ret);
}

static void *do_modify(void *arg)
{
	memset(buf, modify_pattern, blocksize);
	pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
	pthread_t io_thread;
	pthread_t modify_thread;
	unsigned long long filepos;
	int opt;
	int ret = -EINVAL;

	while ((opt = getopt(argc, argv, "b:s:")) > 0) {
		switch (opt) {
		case 'b':
			blocksize = atoi(optarg);
			if (blocksize == 0) {
				fprintf(stderr, "invalid blocksize '%s'\n", optarg);
				goto error;
			}
			break;
		case 's':
			filesize = atoll(optarg);
			if (filesize == 0) {
				fprintf(stderr, "invalid filesize '%s'\n", optarg);
				goto error;
			}
			break;
		default:
			fprintf(stderr, "unknown option '%c'\n", opt);
			goto error;
		}
	}
	if (optind >= argc) {
		fprintf(stderr, "missing argument\n");
		goto error;
	}
	ret = posix_memalign(&buf, sysconf(_SC_PAGESIZE), blocksize);
	if (!buf) {
		fprintf(stderr, "failed to allocate aligned memory\n");
		exit(EXIT_FAILURE);
	}
	fd = open(argv[optind], O_DIRECT | O_WRONLY | O_CREAT, 0600);
	if (fd < 0) {
		fprintf(stderr, "failed to open file '%s': %m\n", argv[optind]);
		goto error;
	}

	for (filepos = 0; filepos < filesize; filepos += blocksize) {
		void *retval = NULL;

		memset(buf, orig_pattern, blocksize);
		pthread_create(&io_thread, NULL, do_io, NULL);
		pthread_create(&modify_thread, NULL, do_modify, NULL);
		ret = pthread_join(io_thread, &retval);
		if (ret) {
			errno = ret;
			ret = -ret;
			fprintf(stderr, "failed to join io thread: %m\n");
			goto error;
		}
		if ((ssize_t )retval < blocksize) {
			ret = -EIO;
			fprintf(stderr, "io thread failed\n");
			goto error;
		}
		ret = pthread_join(modify_thread, NULL);
		if (ret) {
			errno = ret;
			ret = -ret;
			fprintf(stderr, "failed to join modify thread: %m\n");
			goto error;
		}
	}
error:
	close(fd);
	free(buf);
	if (ret < 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
