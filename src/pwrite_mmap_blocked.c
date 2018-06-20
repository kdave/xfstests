// SPDX-License-Identifier: GPL-2.0
/*    Copyright (c) 2010 Intel Corporation
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>



int main(int argc, char *argv[])
{
	int ret;
	char *cc = "01234";
	char *progname;
	loff_t size;
	loff_t amount = 1;
	loff_t from = 2;
	loff_t to = 3;
	int fd;
	void *mapped_mem;

	progname = argv[0];
	size = 5;
	fd = open(argv[1], O_RDWR|O_TRUNC|O_CREAT, 0666);
	if (fd < 0) {
		fprintf(stderr, "%s: Cannot open `%s': %s\n",
			progname, argv[1], strerror(errno));
		exit(1);
	}

	if ((ret = pwrite(fd, (const char *)cc,
				size, 0)) != size) {
		perror("pwrite");
		exit(1);
	}

	mapped_mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapped_mem == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	printf("pwrite %Ld bytes from %Ld to %Ld\n",
		(long long) amount, (long long) from, (long long) to);

	ret = pwrite(fd, (char *)mapped_mem + from, amount, to);
	if (ret != amount) {
		perror("pwrite");
		exit(1);
	}

	munmap(mapped_mem,0);
	close(fd);
	exit(0);
}
