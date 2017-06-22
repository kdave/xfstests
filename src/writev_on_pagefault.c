/*
 * Takes page fault while writev is iterating over the vectors in the IOV
 *
 * Copyright (C) 2017 Red Hat, Inc. All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <limits.h>

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
#define DEF_IOV_CNT 3


void usage(char *progname)
{
	fprintf(stderr, "usage: %s [-i iovcnt] filename\n", progname);
	fprintf(stderr, "\t-i iovcnt: the count of io vectors (max is 1024), 3 by default\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd, i, c;
	size_t ret;
	struct iovec *iov;
	int pagesz = 4096;
	char *data = NULL;
	char *filename = NULL;
	int iov_cnt = DEF_IOV_CNT;


	while ((c = getopt(argc, argv, "i:")) != -1) {
		char *endp;

		switch (c) {
		case 'i':
			iov_cnt = strtol(optarg, &endp, 0);
			if (*endp) {
				fprintf(stderr, "Invalid iov count %s\n", optarg);
				usage(argv[0]);
			}
			break;
		default:
			usage(argv[0]);
		}
	}

	if (iov_cnt > IOV_MAX || iov_cnt <= 0)
		usage(argv[0]);

	if (optind == argc - 1)
		filename = argv[optind];
	else
		usage(argv[0]);

	pagesz = getpagesize();
	data = malloc(pagesz * iov_cnt);
	if (!data) {
		perror("malloc failed");
		return 1;
	}

	/*
	 * NOTE: no pre-writing/reading on the buffer before writev, to prevent
	 * page prefault from happening, we need it happen at writev time.
	 */

	iov = calloc(iov_cnt, sizeof(struct iovec));
	if (!iov) {
		perror("calloc failed");
		return 1;
	}

	for (i = 0; i < iov_cnt; i++) {
		(iov + i)->iov_base = (data + pagesz * i);
		(iov + i)->iov_len  = 1;
	}

	if ((fd = open(filename, O_TRUNC|O_CREAT|O_RDWR, 0644)) < 0) {
		perror("open failed");
		return 1;
	}

	ret = writev(fd, iov, iov_cnt);
	if (ret < 0)
		perror("writev failed");
	else
		printf("wrote %d bytes\n", (int)ret);

	close(fd);
	return 0;
}
