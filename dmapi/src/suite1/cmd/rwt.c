/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#ifdef linux
#include <string.h>
#endif

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
	int	i;

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
	int		i;

	if (Progname = strrchr(argv[0], '/')) {
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
			fprintf(stderr, "malloc of %d bytes failed\n", length);
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
#ifdef __sgi
				"seeked to offset %lld, actually "
				"arrived at %lld\n",
				(int64_t)offset, (int64_t)seek_off);
#else
				"seeked to offset %d, actually "
				"arrived at %d\n",
				offset, seek_off);
#endif
			exit(1);
		}
	}

	if (wflag) {
		if ((rc = write(fd, bufp, length)) < 0) {
			fprintf(stderr, "write failed, %s\n", strerror(errno));
			exit(1);
		}
		if (rc != length) {
			fprintf(stderr, "expected to write %d bytes, actually "
				"wrote %d bytes\n", length, rc);
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
			fprintf(stderr, "expected to read %d bytes, actually "
				"read %d bytes\n", length, rc);
			exit(1);
		}
	}
	exit(0);
}
