// SPDX-License-Identifier: GPL-2.0-or-newer
/*
 * Copyright (c) 2019 Oracle.
 * All Rights Reserved.
 *
 * Create writable mappings to multiple files and write them all to test
 * concurrent mmap writes to the same shared blocks.
 */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct file_info {
	char			*mapping;
	off_t			file_offset;
	off_t			file_length;
	int			fd;
};

int
main(
	int			argc,
	char			*argv[])
{
	struct file_info	*fi;
	size_t			length;
	char			*endptr;
	unsigned int		nr_files;
	unsigned int		i;
	char			*buf;
	int			ret;

	if (argc < 4) {
		printf("Usage: %s len offset file [offset file]...\n", argv[0]);
		return 1;
	}

	/* Parse mwrite length. */
	errno = 0;
	length = strtoul(argv[1], &endptr, 0);
	if (errno) {
		perror(argv[1]);
		return 1;
	}
	if (*endptr != '\0') {
		fprintf(stderr, "%s: not a proper file length?\n", argv[2]);
		return 1;
	}

	/* Allocate file info */
	nr_files = (argc - 2) / 2;

	fi = calloc(nr_files, sizeof(struct file_info));
	if (!fi) {
		perror("calloc file info");
		return 1;
	}

	buf = malloc(length);
	if (!buf) {
		perror("malloc buf");
		return 1;
	}

	for (i = 0; i < nr_files; i++) {
		struct stat	statbuf;
		char		*offset = argv[((i + 1) * 2)];
		char		*fname = argv[((i + 1) * 2) + 1];

		/* Open file, create mapping for the range we want. */
		fi[i].fd = open(fname, O_RDWR);
		if (fi[i].fd < 0) {
			perror(fname);
			return 1;
		}

		/* Parse mwrite offset */
		errno = 0;
		fi[i].file_offset = strtoul(offset, &endptr, 0);
		if (errno) {
			perror(argv[1]);
			return 1;
		}

		/* Remember file size */
		ret = fstat(fi[i].fd, &statbuf);
		if (ret) {
			perror(fname);
			return 1;
		}
		fi[i].file_length = statbuf.st_size;

		if (fi[i].file_offset + length > fi[i].file_length) {
			fprintf(stderr, "%s: file must be %llu bytes\n",
				fname,
				(unsigned long long)fi[i].file_offset + length);
			return 1;
		}

		/* Create the mapping */
		fi[i].mapping = mmap(NULL, fi[i].file_length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				fi[i].fd, 0);
		if (fi[i].mapping == MAP_FAILED) {
			perror(fname);
			return 1;
		}

		/*
		 * Make sure the mapping for region we're going to write is
		 * already populated in the page cache.
		 */
		memcpy(buf, fi[i].mapping + fi[i].file_offset, length);
	}

	/* Dirty the same region in each file to test COW. */
	for (i = 0; i < nr_files; i++) {
		memset(buf, 0x62 + i, length);
		memcpy(fi[i].mapping + fi[i].file_offset, buf, length);
	}
	for (i = 0; i < nr_files; i++) {
		ret = msync(fi[i].mapping, fi[i].file_offset + length, MS_SYNC);
		if (ret) {
			perror("msync");
			return 1;
		}
	}

	/* Close everything. */
	for (i = 0; i < nr_files; i++) {
		ret = munmap(fi[i].mapping, fi[i].file_length);
		if (ret) {
			perror("munmap");
			return 1;
		}

		ret = close(fi[i].fd);
		if (ret) {
			perror("close");
			return 1;
		}
	}

	/* Free everything. */
	free(buf);
	free(fi);

	return 0;
}
