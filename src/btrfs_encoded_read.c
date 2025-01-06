// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <linux/btrfs.h>
#include <liburing.h>
#include "config.h"

/* IORING_OP_URING_CMD defined from liburing 2.2 onwards */
#if LIBURING_MAJOR_VERSION < 2 || (LIBURING_MAJOR_VERSION == 2 && LIBURING_MINOR_VERSION < 2)
#define IORING_OP_URING_CMD 46
#endif

#ifndef BTRFS_IOC_ENCODED_READ
struct btrfs_ioctl_encoded_io_args {
	const struct iovec *iov;
	unsigned long iovcnt;
	__s64 offset;
	__u64 flags;
	__u64 len;
	__u64 unencoded_len;
	__u64 unencoded_offset;
	__u32 compression;
	__u32 encryption;
	__u8 reserved[64];
};

#define BTRFS_IOC_ENCODED_READ _IOR(BTRFS_IOCTL_MAGIC, 64, struct btrfs_ioctl_encoded_io_args)
#endif

#define BTRFS_MAX_COMPRESSED 131072
#define QUEUE_DEPTH 1

static int encoded_read_ioctl(const char *filename, long long offset)
{
	int ret, fd;
	char buf[BTRFS_MAX_COMPRESSED];
	struct iovec iov;
	struct btrfs_ioctl_encoded_io_args enc;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open failed for %s\n", filename);
		return 1;
	}

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	enc.iov = &iov;
	enc.iovcnt = 1;
	enc.offset = offset;
	enc.flags = 0;

	ret = ioctl(fd, BTRFS_IOC_ENCODED_READ, &enc);

	if (ret < 0) {
		printf("%i\n", -errno);
		close(fd);
		return 0;
	}

	close(fd);

	printf("%i\n", ret);
	printf("%llu\n", enc.len);
	printf("%llu\n", enc.unencoded_len);
	printf("%llu\n", enc.unencoded_offset);
	printf("%u\n", enc.compression);
	printf("%u\n", enc.encryption);

	fwrite(buf, ret, 1, stdout);

	return 0;
}

static int encoded_read_io_uring(const char *filename, long long offset)
{
	int ret, fd;
	char buf[BTRFS_MAX_COMPRESSED];
	struct iovec iov;
	struct btrfs_ioctl_encoded_io_args enc;
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;

	io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open failed for %s\n", filename);
		ret = 1;
		goto out_uring;
	}

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	enc.iov = &iov;
	enc.iovcnt = 1;
	enc.offset = offset;
	enc.flags = 0;

	sqe = io_uring_get_sqe(&ring);
	if (!sqe) {
		fprintf(stderr, "io_uring_get_sqe failed\n");
		ret = 1;
		goto out_close;
	}

	io_uring_prep_rw(IORING_OP_URING_CMD, sqe, fd, &enc, sizeof(enc), 0);

	/* sqe->cmd_op union'd to sqe->off from liburing 2.3 onwards */
#if LIBURING_MAJOR_VERSION < 2 || (LIBURING_MAJOR_VERSION == 2 && LIBURING_MINOR_VERSION < 3)
	sqe->off = BTRFS_IOC_ENCODED_READ;
#else
	sqe->cmd_op = BTRFS_IOC_ENCODED_READ;
#endif

	io_uring_submit(&ring);

	ret = io_uring_wait_cqe(&ring, &cqe);
	if (ret < 0) {
		fprintf(stderr, "io_uring_wait_cqe returned %i\n", ret);
		ret = 1;
		goto out_close;
	}

	io_uring_cqe_seen(&ring, cqe);

	if (cqe->res < 0) {
		printf("%i\n", cqe->res);
		ret = 0;
		goto out_close;
	}

	printf("%i\n", cqe->res);
	printf("%llu\n", enc.len);
	printf("%llu\n", enc.unencoded_len);
	printf("%llu\n", enc.unencoded_offset);
	printf("%u\n", enc.compression);
	printf("%u\n", enc.encryption);

	fwrite(buf, cqe->res, 1, stdout);

	ret = 0;

out_close:
	close(fd);

out_uring:
	io_uring_queue_exit(&ring);

	return ret;
}

static void usage()
{
	fprintf(stderr, "Usage: btrfs_encoded_read ioctl|io_uring filename offset\n");
}

int main(int argc, char *argv[])
{
	const char *filename;
	long long offset;

	if (argc != 4) {
		usage();
		return 1;
	}

	filename = argv[2];

	offset = atoll(argv[3]);
	if (offset == 0 && errno != 0) {
		usage();
		return 1;
	}

	if (!strcmp(argv[1], "ioctl")) {
		return encoded_read_ioctl(filename, offset);
	} else if (!strcmp(argv[1], "io_uring")) {
		return encoded_read_io_uring(filename, offset);
	} else {
		usage();
		return 1;
	}
}
