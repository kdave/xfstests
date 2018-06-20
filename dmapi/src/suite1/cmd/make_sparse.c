// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/*
 *
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

char *	Progname;


static void
Usage(void)
{
	fprintf(stderr,"Usage: %s filename\n", Progname);
	exit(1);
}


int
main(
	int	argc,
	char	**argv)
{
	char	*pathname;
	u_int	buflen;
	char	*buf;
	ssize_t	offset;
	ssize_t	count;
	int	fd;
	int	i;

	Progname = argv[0];

	if (argc != 2)
		Usage();
	pathname = argv[1];

	/* Create the file and make it a regular file. */

	if ((fd = open(pathname, O_RDWR|O_CREAT|O_EXCL, 0600)) < 0) {
		fprintf(stderr,"%s: Cannot open %s, %s\n", Progname,
			pathname, strerror(errno));
		exit(1);
	}

	/* Malloc and zero a buffer to use for writes. */

	buflen = 1;
	if ((buf = malloc(buflen)) == NULL) {
		fprintf(stderr,"%s: malloc(%d) returned NULL\n",
			Progname, buflen);
		exit(1);
	}
	memset(buf, '\0', buflen);

	for (i = 0; i < 200; i += 2) {
		offset = i * 65536;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			fprintf(stderr, "seek to %zd failed, %s\n", offset,
				strerror(errno));
			exit(1);
		}
		if ((count = write(fd, buf, buflen)) < 0) {
			fprintf(stderr, "write of %d bytes failed at offset "
				"%zd, , %s\n", buflen, offset, strerror(errno));
			exit(1);
		}
		if (count != buflen) {
			fprintf(stderr, "expected to write %d bytes at offset "
				"%zd, actually wrote %zd\n", buflen, offset,
				count);
			exit(1);
		}
	}
	exit(0);
}
