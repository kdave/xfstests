/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <string.h>

/*---------------------------------------------------------------------------

Test program used to test DMAPI by issuing read, write, and trunc calls to a
file.  The command line is:

	rwt [-r|-w|-t] [-o offset] [-l length] pathname

where:
-r
	indicates that a read should be done (the default if none specified)
-w
	indiates that a write should be done
-t
	indicates that a truncate should be done, in which case the -l
	parameter is ignored.
-o offset
	offset at which to begin the read, write or truncate (default is 0).
-l length
	the length in bytes to read or write (default is 1).
pathname
	the file to be used by the test.

----------------------------------------------------------------------------*/

#ifndef linux
extern	char	*sys_errlist[];
#endif
extern  int     optind;
extern  char    *optarg;


char	*Progname;


static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-r|-w|-t] [-o offset] [-l length] "
		"pathname\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	char		*pathname = NULL;
	off_t		offset = 0;
	size_t		length = 1;
	u_char		ch = 'X';
	void		*bufp = NULL;
	off_t		seek_off;
	int		rflag = 0;
	int		wflag = 0;
	int		tflag = 0;
	int		fd;
	ssize_t		rc;
	int		opt;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "rwto:l:")) != EOF) {
		switch (opt) {
		case 'r':
			rflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'o':
			offset = atol(optarg);
			break;
		case 'l':
			length = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	if (rflag + wflag + tflag > 1)
		usage();
	pathname = argv[optind];

	if ((fd = open(pathname, O_RDWR)) < 0) {
		fprintf(stderr, "open of %s failed, %s\n", pathname,
			strerror(errno));
		exit(1);
	}
	if (length > 0) {
		if ((bufp = malloc(length)) == NULL) {
			fprintf(stderr, "malloc of %zd bytes failed\n", length);
			exit(1);
		}
		if (wflag)
			memset(bufp, ch, length);
	}

	if (!tflag) {
		if ((seek_off = lseek(fd, offset, SEEK_SET)) < 0) {
			fprintf(stderr, "seek failed, %s\n", strerror(errno));
			exit(1);
		}
		if (seek_off != offset) {
			fprintf(stderr,
				"seeked to offset %lld, actually "
				"arrived at %lld\n",
				(long long) offset, (long long) seek_off);
			exit(1);
		}
	}

	if (wflag) {
		if ((rc = write(fd, bufp, length)) < 0) {
			fprintf(stderr, "write failed, %s\n", strerror(errno));
			exit(1);
		}
		if (rc != length) {
			fprintf(stderr, "expected to write %zd bytes, actually "
				"wrote %zd bytes\n", length, rc);
			exit(1);
		}
	} else if (tflag) {
		if (ftruncate(fd, offset) != 0) {
			fprintf(stderr, "truncate failed, %s\n",
				strerror(errno));
			exit(1);
		}
	} else {
		if ((rc = read(fd, bufp, length)) < 0) {
			fprintf(stderr, "read failed, %s\n", strerror(errno));
			exit(1);
		}
		if (rc != length) {
			fprintf(stderr, "expected to read %zd bytes, actually "
				"read %zd bytes\n", length, rc);
			exit(1);
		}
	}
	exit(0);
}
