// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 *
 * Race pwrite/mwrite with dedupe to see if we got the locking right.
 *
 * File writes and mmap writes should not be able to change the src_fd's
 * contents after dedupe prep has verified that the file contents are the same.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

#define GOOD_BYTE		0x58
#define BAD_BYTE		0x66

#ifndef FIDEDUPERANGE
/* extent-same (dedupe) ioctls; these MUST match the btrfs ioctl definitions */
#define FILE_DEDUPE_RANGE_SAME		0
#define FILE_DEDUPE_RANGE_DIFFERS	1

/* from struct btrfs_ioctl_file_extent_same_info */
struct file_dedupe_range_info {
	__s64 dest_fd;		/* in - destination file */
	__u64 dest_offset;	/* in - start of extent in destination */
	__u64 bytes_deduped;	/* out - total # of bytes we were able
				 * to dedupe from this file. */
	/* status of this dedupe operation:
	 * < 0 for error
	 * == FILE_DEDUPE_RANGE_SAME if dedupe succeeds
	 * == FILE_DEDUPE_RANGE_DIFFERS if data differs
	 */
	__s32 status;		/* out - see above description */
	__u32 reserved;		/* must be zero */
};

/* from struct btrfs_ioctl_file_extent_same_args */
struct file_dedupe_range {
	__u64 src_offset;	/* in - start of extent in source */
	__u64 src_length;	/* in - length of extent */
	__u16 dest_count;	/* in - total elements in info array */
	__u16 reserved1;	/* must be zero */
	__u32 reserved2;	/* must be zero */
	struct file_dedupe_range_info info[0];
};
#define FIDEDUPERANGE	_IOWR(0x94, 54, struct file_dedupe_range)
#endif /* FIDEDUPERANGE */

static int fd1, fd2;
static loff_t offset = 37; /* Nice low offset to trick the compare */
static loff_t blksz;

/* Continuously dirty the pagecache for the region being dupe-tested. */
void *
mwriter(
	void		*data)
{
	volatile char	*p;

	p = mmap(NULL, blksz, PROT_WRITE, MAP_SHARED, fd1, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(2);
	}

	while (1) {
		*(p + offset) = BAD_BYTE;
		*(p + offset) = GOOD_BYTE;
	}
}

/* Continuously write to the region being dupe-tested. */
void *
pwriter(
	void		*data)
{
	char		v;
	ssize_t		sz;

	while (1) {
		v = BAD_BYTE;
		sz = pwrite(fd1, &v, sizeof(v), offset);
		if (sz != sizeof(v)) {
			perror("pwrite0");
			exit(2);
		}

		v = GOOD_BYTE;
		sz = pwrite(fd1, &v, sizeof(v), offset);
		if (sz != sizeof(v)) {
			perror("pwrite1");
			exit(2);
		}
	}

	return NULL;
}

static inline void
complain(
	loff_t	offset,
	char	bad)
{
	fprintf(stderr, "ASSERT: offset %llu should be 0x%x, got 0x%x!\n",
			(unsigned long long)offset, GOOD_BYTE, bad);
	abort();
}

/* Make sure the destination file pagecache never changes. */
void *
mreader(
	void		*data)
{
	volatile char	*p;

	p = mmap(NULL, blksz, PROT_READ, MAP_SHARED, fd2, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(2);
	}

	while (1) {
		if (*(p + offset) != GOOD_BYTE)
			complain(offset, *(p + offset));
	}
}

/* Make sure the destination file never changes. */
void *
preader(
	void		*data)
{
	char		v;
	ssize_t		sz;

	while (1) {
		sz = pread(fd2, &v, sizeof(v), offset);
		if (sz != sizeof(v)) {
			perror("pwrite0");
			exit(2);
		}

		if (v != GOOD_BYTE)
			complain(offset, v);
	}

	return NULL;
}

void
print_help(const char *progname)
{
	printf("Usage: %s [-b blksz] [-c dir] [-n nr_ops] [-o offset] [-r] [-w] [-v]\n",
			progname);
	printf("-b sets the block size (default is autoconfigured)\n");
	printf("-c chdir to this path before starting\n");
	printf("-n controls the number of dedupe ops (default 10000)\n");
	printf("-o reads and writes to this offset (default 37)\n");
	printf("-r uses pread instead of mmap read.\n");
	printf("-v prints status updates.\n");
	printf("-w uses pwrite instead of mmap write.\n");
}

int
main(
	int		argc,
	char		*argv[])
{
	struct file_dedupe_range *fdr;
	char		*Xbuf;
	void		*(*reader_fn)(void *) = mreader;
	void		*(*writer_fn)(void *) = mwriter;
	unsigned long	same = 0;
	unsigned long	differs = 0;
	unsigned long	i, nr_ops = 10000;
	ssize_t		sz;
	pthread_t	reader, writer;
	int		verbose = 0;
	int		c;
	int		ret;

	while ((c = getopt(argc, argv, "b:c:n:o:rvw")) != -1) {
		switch (c) {
		case 'b':
			errno = 0;
			blksz = strtoul(optarg, NULL, 0);
			if (errno) {
				perror(optarg);
				exit(1);
			}
			break;
		case 'c':
			ret = chdir(optarg);
			if (ret) {
				perror("chdir");
				exit(1);
			}
			break;
		case 'n':
			errno = 0;
			nr_ops = strtoul(optarg, NULL, 0);
			if (errno) {
				perror(optarg);
				exit(1);
			}
			break;
		case 'o':
			errno = 0;
			offset = strtoul(optarg, NULL, 0);
			if (errno) {
				perror(optarg);
				exit(1);
			}
			break;
		case 'r':
			reader_fn = preader;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			writer_fn = pwriter;
			break;
		default:
			print_help(argv[0]);
			exit(1);
			break;
		}
	}

	fdr = malloc(sizeof(struct file_dedupe_range) +
			sizeof(struct file_dedupe_range_info));
	if (!fdr) {
		perror("malloc");
		exit(1);
	}

	/* Initialize both files. */
	fd1 = open("file1", O_RDWR | O_CREAT | O_TRUNC | O_NOATIME, 0600);
	if (fd1 < 0) {
		perror("file1");
		exit(1);
	}

	fd2 = open("file2", O_RDWR | O_CREAT | O_TRUNC | O_NOATIME, 0600);
	if (fd2 < 0) {
		perror("file2");
		exit(1);
	}

	if (blksz <= 0) {
		struct stat	statbuf;

		ret = fstat(fd1, &statbuf);
		if (ret) {
			perror("file1 stat");
			exit(1);
		}
		blksz = statbuf.st_blksize;
	}

	if (offset >= blksz) {
		fprintf(stderr, "offset (%llu) < blksz (%llu)?\n",
				(unsigned long long)offset,
				(unsigned long long)blksz);
		exit(1);
	}

	Xbuf = malloc(blksz);
	if (!Xbuf) {
		perror("malloc buffer");
		exit(1);
	}
	memset(Xbuf, GOOD_BYTE, blksz);

	sz = pwrite(fd1, Xbuf, blksz, 0);
	if (sz != blksz) {
		perror("file1 write");
		exit(1);
	}

	sz = pwrite(fd2, Xbuf, blksz, 0);
	if (sz != blksz) {
		perror("file2 write");
		exit(1);
	}

	ret = fsync(fd1);
	if (ret) {
		perror("file1 fsync");
		exit(1);
	}

	ret = fsync(fd2);
	if (ret) {
		perror("file2 fsync");
		exit(1);
	}

	/* Start our reader and writer threads. */
	ret = pthread_create(&reader, NULL, reader_fn, NULL);
	if (ret) {
		fprintf(stderr, "rthread: %s\n", strerror(ret));
		exit(1);
	}

	ret = pthread_create(&writer, NULL, writer_fn, NULL);
	if (ret) {
		fprintf(stderr, "wthread: %s\n", strerror(ret));
		exit(1);
	}

	/*
	 * Now start deduping.  If the contents match, fd1's blocks will be
	 * remapped into fd2, which is why the writer thread targets fd1 and
	 * the reader checks fd2 to make sure that none of fd1's writes ever
	 * make it into fd2.
	 */
	for (i = 1; i <= nr_ops; i++) {
		fdr->src_offset = 0;
		fdr->src_length = blksz;
		fdr->dest_count = 1;
		fdr->reserved1 = 0;
		fdr->reserved2 = 0;
		fdr->info[0].dest_fd = fd2;
		fdr->info[0].dest_offset = 0;
		fdr->info[0].reserved = 0;

		ret = ioctl(fd1, FIDEDUPERANGE, fdr);
		if (ret) {
			perror("dedupe");
			exit(2);
		}

		switch (fdr->info[0].status) {
		case FILE_DEDUPE_RANGE_DIFFERS:
			differs++;
			break;
		case FILE_DEDUPE_RANGE_SAME:
			same++;
			break;
		default:
			fprintf(stderr, "deduperange: %s\n",
					strerror(-fdr->info[0].status));
			exit(2);
			break;
		}

		if (verbose && (i % 337) == 0)
			printf("nr_ops: %lu; same: %lu; differs: %lu\n",
					i, same, differs);
	}

	if (verbose)
		printf("nr_ops: %lu; same: %lu; differs: %lu\n", i - 1, same,
				differs);

	/* Program termination will kill the threads and close the files. */
	return 0;
}
