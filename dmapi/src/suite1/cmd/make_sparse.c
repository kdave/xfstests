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
			fprintf(stderr, "seek to %d failed, %s\n", offset,
				strerror(errno));
			exit(1);
		}
		if ((count = write(fd, buf, buflen)) < 0) {
			fprintf(stderr, "write of %d bytes failed at offset "
				"%d, , %s\n", buflen, offset, strerror(errno));
			exit(1);
		}
		if (count != buflen) {
			fprintf(stderr, "expected to write %d bytes at offset "
				"%d, actually wrote %d\n", buflen, offset,
				count);
			exit(1);
		}
	}
	exit(0);
}
