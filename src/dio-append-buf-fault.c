// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 SUSE Linux Products GmbH.  All Rights Reserved.
 */

/*
 * Test a direct IO write in append mode with a buffer that was not faulted in
 * (or just partially) before the write.
 */

/* Get the O_DIRECT definition. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

static ssize_t do_write(int fd, const void *buf, size_t count)
{
        while (count > 0) {
		ssize_t ret;

		ret = write(fd, buf, count);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return ret;
		}
		count -= ret;
		buf += ret;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct stat stbuf;
	int fd;
	long pagesize;
	void *buf;
	ssize_t ret;

	if (argc != 2) {
		fprintf(stderr, "Use: %s <file path>\n", argv[0]);
		return 1;
	}

	/*
	 * First try an append write against an empty file of a buffer with a
	 * size matching the page size. The buffer is not faulted in before
	 * attempting the write.
	 */

	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT | O_APPEND, 0666);
	if (fd == -1) {
		perror("Failed to open/create file");
		return 2;
	}

	pagesize = sysconf(_SC_PAGE_SIZE);
	if (pagesize == -1) {
		perror("Failed to get page size");
		return 3;
	}

	buf = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		perror("Failed to allocate first buffer");
		return 4;
	}

	ret = do_write(fd, buf, pagesize);
	if (ret < 0) {
		perror("First write failed");
		return 5;
	}

	ret = fstat(fd, &stbuf);
	if (ret < 0) {
		perror("First stat failed");
		return 6;
	}

	/* Don't exit on failure so that we run the second test below too. */
	if (stbuf.st_size != pagesize)
		fprintf(stderr,
			"Wrong file size after first write, got %jd expected %ld\n",
			(intmax_t)stbuf.st_size, pagesize);

	munmap(buf, pagesize);
	close(fd);

	/*
	 * Now try an append write against an empty file of a buffer with a
	 * size matching twice the page size. Only the first page of the buffer
	 * is faulted in before attempting the write, so that the second page
	 * should be faulted in during the write.
	 */
	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT | O_APPEND, 0666);
	if (fd == -1) {
		perror("Failed to open/create file");
		return 7;
	}

	buf = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		perror("Failed to allocate second buffer");
		return 8;
	}

	/* Fault in first page of the buffer before the write. */
	memset(buf, 0, 1);

	ret = do_write(fd, buf, pagesize * 2);
	if (ret < 0) {
		perror("Second write failed");
		return 9;
	}

	ret = fstat(fd, &stbuf);
	if (ret < 0) {
		perror("Second stat failed");
		return 10;
	}

	if (stbuf.st_size != pagesize * 2)
		fprintf(stderr,
			"Wrong file size after second write, got %jd expected %ld\n",
			(intmax_t)stbuf.st_size, pagesize * 2);

	munmap(buf, pagesize * 2);
	close(fd);

	return 0;
}
