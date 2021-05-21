// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 RedHat Inc.  All Rights Reserved.
 *
 * mmap a file, alloc blocks, reading&writing blocks with overlapping. For example:
 *
 * |<--- block --->|<--- block --->|
 *  len             len
 * +---------------+---------------+
 * |AAAA| ........ |AAAA| ........ |
 * +---------------+---------------+
 *    |               |
 *    |               `------------+
 *    `-----------------------+    |
 *                            |    |
 *                            V    V
 * +---------------+---------------+----+
 * |AAAA| ........ |AAAA| ... |AAAA|AAAA|
 * +---------------+---------------+----+
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

void usage(char *progname)
{
	fprintf(stderr, "usage: %s [-b blocksize ] [-c count] [-l length] filename\n"
	        "\tmmap $count * $blocksize bytes memory, pwritev $length bytes in each block. blocksize=4096, count=2, length=12 by default.\n"
                "e.g: %s -b 4096 -c 2 -l 12 filename\n",
	        progname, progname);
        exit(1);
}

int main(int argc, char **argv)
{
	char *filename = NULL;
	size_t bsize = 4096;
	size_t count = 2;
	size_t length = 12;
	int fd, i, c;
	void *base;
	char *buf, *cmp_buf;
	struct iovec *iov;
	int ret = 0;

	while ((c = getopt(argc, argv, "b:l:c:")) != -1) {
		char *endp;

		switch (c) {
		case 'b':
			bsize = strtoul(optarg, &endp, 0);
			break;
		case 'c':
			count = strtoul(optarg, &endp, 0);
			break;
		case 'l':
			length = strtoul(optarg, &endp, 0);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind == argc - 1)
		filename = argv[optind];
	else
		usage(argv[0]);

	if (length >= bsize) {
		printf("-l length must be less than -b blocksize\n");
		usage(argv[0]);
	}

	fd = open(filename, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "open %s failed:%s\n", filename, strerror(errno));
		exit(1);
	}
	base = mmap(NULL, bsize * count, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED) {
		fprintf(stderr, "mmap failed %s\n", strerror(errno));
		exit(1);
	}

	/* Write each of blocks */
	buf = malloc(length);
	memset(buf, 0xAA, length);
	for (i=0; i<count; i++) {
		ret = pwrite(fd, buf, length, i * bsize);
		if (ret == -1) {
			fprintf(stderr, "pwrite failed %s\n", strerror(errno));
			exit(1);
		}
	}

	/* Copy from the beginning of each blocks ... */
	iov = malloc(sizeof(struct iovec) * count);
	for (i=0; i<count; i++) {
		iov[i].iov_base = base + i * bsize;
		iov[i].iov_len  = length;
	}
	/* ... Write to the last block with offset ($bsize - $length) */
	ret = pwritev(fd, iov, count, bsize * count - length);
	if (ret == -1) {
		fprintf(stderr, "pwritev failed %s\n", strerror(errno));
		exit(1);
	}

	/* Verify data */
	cmp_buf = malloc(length);
	for (i=0; i<count; i++) {
		ret = pread(fd, cmp_buf, length, bsize * count + (i - 1) * length);
		if (ret == -1) {
			fprintf(stderr, "pread failed %s\n", strerror(errno));
			exit(1);
		}
		if (memcmp(buf, cmp_buf, length))
			printf("Find corruption\n");
	}

	munmap(base, bsize * count);
	free(buf);
	free(cmp_buf);
	free(iov);
	close(fd);

	return 0;
}
