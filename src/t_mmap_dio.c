/*
 * This programme was originally written by
 *     Jeff Moyer <jmoyer@redhat.com>
 *
 * Copyright (C) 2016, Red Hat, Inc.
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libaio.h>
#include <errno.h>
#include <sys/time.h>

void usage(char *prog)
{
	fprintf(stderr,
		"usage: %s <src file> <dest file> <size> <msg>\n",
		prog);
	exit(1);
}

void err_exit(char *op, unsigned long len, char *s)
{
	fprintf(stderr, "%s(%s) len %lu %s\n",
		op, strerror(errno), len, s);
	exit(1);
}

int main(int argc, char **argv)
{
	int fd, fd2, ret, dio = 1;
	char *map;
	char *msg;
	char *sfile;
	char *dfile;
	unsigned long len, opt;

	if (argc < 5)
		usage(basename(argv[0]));

	while ((opt = getopt(argc, argv, "b")) != -1)
		dio = 0;

	sfile = argv[optind];
	dfile = argv[optind + 1];
	msg = argv[optind + 3];
	len = strtoul(argv[optind + 2], NULL, 10);
	if (errno == ERANGE)
		err_exit("strtoul", 0, msg);

	/* Open source file and mmap*/
	fd = open(sfile, O_RDWR, 0644);
	if (fd < 0)
		err_exit("open src", len, msg);

	map = (char *)mmap(NULL, len,
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED)
		err_exit("mmap", len, msg);

	if (dio == 1) {
		/* Open dest file with O_DIRECT */
		fd2 = open(dfile, O_RDWR|O_DIRECT, 0644);
		if (fd2 < 0)
			err_exit("open dest", len, msg);
	} else {
		/* Open dest file without O_DIRECT */
		fd2 = open(dfile, O_RDWR, 0644);
		if (fd2 < 0)
			err_exit("open dest", len, msg);
	}

	/* First, test storing to dest file from source mapping */
	ret = write(fd2, map, len);
	if (ret != len)
		err_exit("write", len, msg);

	ret = fsync(fd2);
	if (ret != 0)
		err_exit("fsync", len, msg);

	ret = (int)lseek(fd2, 0, SEEK_SET);
	if (ret == -1)
		err_exit("lseek", len, msg);

	/* Next, test reading from dest file into source mapping */
	ret = read(fd2, map, len);
	if (ret != len)
		err_exit("read", len, msg);
	ret = msync(map, len, MS_SYNC);
	if (ret < 0)
		err_exit("msync", len, msg);

	ret = munmap(map, len);
	if (ret < 0)
		err_exit("munmap", len, msg);

	ret = close(fd);
	if (ret < 0)
		err_exit("clsoe fd", len, msg);

	ret = close(fd2);
	if (ret < 0)
		err_exit("close fd2", len, msg);

	exit(0);
}
