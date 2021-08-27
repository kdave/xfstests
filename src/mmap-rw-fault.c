// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Red Hat, Inc.  All Rights Reserved.
 * Written by Andreas Gruenbacher (agruenba@redhat.com)
 */

/* Trigger page faults in the same file during read and write. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* to get definition of O_DIRECT flag. */
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

char *filename;
unsigned int page_size;
void *page;
char *addr;
int fd;
ssize_t ret;

/*
 * Leave a hole at the beginning of the test file and initialize a block of
 * @page_size bytes at offset @page_size to @c.  Then, reopen the file and
 * mmap the first two pages.
 */
void init(char c, int flags)
{
	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_DIRECT, 0666);
	if (fd == -1)
		goto fail;
	memset(page, c, page_size);
	ret = pwrite(fd, page, page_size, page_size);
	if (ret != page_size)
		goto fail;
	if (close(fd))
		goto fail;

	fd = open(filename, flags);
	if (fd == -1)
		goto fail;
	addr = mmap(NULL, 2 * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		err(1, NULL);
	return;

fail:
	err(1, "%s", filename);
}

void done(void)
{
	if (fsync(fd))
		goto fail;
	if (close(fd))
		goto fail;
	return;

fail:
	err(1, "%s", filename);
}

static ssize_t do_read(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t count2 = 0, ret;

	do {
		ret = pread(fd, buf, count, offset);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ret == 0)
			break;
		count2 += ret;
		buf += ret;
		count -= ret;
	} while (count);
	return count2;
}

static ssize_t do_write(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t count2 = 0, ret;

	do {
		ret = pwrite(fd, buf, count, offset);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ret == 0)
			break;
		count2 += ret;
		buf += ret;
		count -= ret;
	} while (count);
	return count2;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
		errx(1, "no test filename argument given");
	filename = argv[1];

	page_size = ret = sysconf(_SC_PAGE_SIZE);
	if (ret == -1)
		err(1, NULL);

	ret = posix_memalign(&page, page_size, page_size);
	if (ret) {
		errno = ENOMEM;
		err(1, NULL);
	}

	/*
	 * Make sure page faults during pread are handled correctly:
	 * read from an allocated area on disk into page 0.
	 */
	init('a', O_RDWR);
	ret = do_read(fd, addr, page_size, page_size);
	if (ret != page_size)
		err(1, "pread %s: %ld != %u", filename, ret, page_size);
	if (memcmp(addr, page, page_size))
		errx(1, "pread is broken");
	done();

	init('b', O_RDWR | O_DIRECT);
	ret = do_read(fd, addr, page_size, page_size);
	if (ret != page_size)
		err(1, "pread %s (O_DIRECT): %ld != %u", filename, ret, page_size);
	if (memcmp(addr, page, page_size))
		errx(1, "pread (D_DIRECT) is broken");
	done();

	/*
	 * Make sure page faults during pwrite are handled correctly:
	 * write from an allocated area on disk into page 0.
	 */
	init('c', O_RDWR);
	ret = do_write(fd, addr + page_size, page_size, 0);
	if (ret != page_size)
		err(1, "pwrite %s: %ld != %u", filename, ret, page_size);
	if (memcmp(addr, page, page_size))
		errx(1, "pwrite is broken");
	done();

	init('d', O_RDWR | O_DIRECT);
	ret = do_write(fd, addr + page_size, page_size, 0);
	if (ret != page_size)
		err(1, "pwrite %s (O_DIRECT): %ld != %u", filename, ret, page_size);
	if (memcmp(addr, page, page_size))
		errx(1, "pwrite (O_DIRECT) is broken");
	done();

	/*
	 * Reading from a hole under O_DIRECT takes a different code path in
	 * the kernel.  Read from a hole into page 0 to test that.  (It
	 * shouldn't matter that the hole and page 0 coincide.)
	 */
	init('e', O_RDWR | O_DIRECT);
	ret = do_read(fd, addr, page_size, 0);
	if (ret != page_size)
		err(1, "pread %s (O_DIRECT) from hole: %ld != %u", filename, ret, page_size);
	memset(page, 0, page_size);
	if (memcmp(addr, page, page_size))
		errx(1, "pread (D_DIRECT) from hole is broken");
	done();

	if (unlink(filename))
		err(1, "unlink %s", filename);

	return 0;
}
