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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

/*
	To read unallocated disk blocks in a filesystem, do
		cc -o test thisfile.c
		./test myfile
		cat myfile
*/

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

	fprintf(stderr, "usage:\t%s [-s size] pathname\n", Progname);
	fprintf(stderr, "\t-s size\t\tsize of file (default 10,000,000 bytes)\n");
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	char	*pathname = NULL;
	off_t	size = 10000000;
	char	buff[1];
	int	method = F_RESVSP;
	flock_t	flock;
	int	opt;
	int	fd;

	if (Progname = strrchr(argv[0], '/')) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "s:")) != EOF) {
		switch (opt) {
		case 's':
			size = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	pathname = argv[optind];

	/* Create the file and write one byte at a large offset to create a
	   big hole in the middle of the file.
	*/

	if ((fd = open(pathname, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		perror("open failed");
		exit(1);
	}
	if (lseek(fd, size, 0) < 0) {
		perror("lseek failed");
		exit(1);
	}
	buff[0] = '\0';
	if (write(fd, buff, 1) != 1) {
		perror("write failed");
		exit(1);
	}

	/* Now fill in the hole with uninitialized blocks. */

	flock.l_whence = 0;
	flock.l_start = 0;
	flock.l_len = size;

	if (fcntl(fd, method, &flock) < 0) {
		perror("fcntl failed");
		exit(1);
	}
	printf("%s created\n", pathname);
}
