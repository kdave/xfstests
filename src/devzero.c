/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <xfs/libxfs.h>

int
main(int argc, char **argv)
{
	off_t		offset = 0;
	int		blksize = 4096;
	long long	maxblks = -1;   /* no limit */
	long long	nblks = 0;
	int		value = 0;
	int		sts = 0;
	int		fd;
	char		*z;
	char		*path;
	int		oflags = O_WRONLY;

	char	*progname;

	if (strrchr(argv[0],'/'))
		progname = strrchr(argv[0],'/')+1;
	else
		progname = argv[0];

	while ((fd = getopt(argc, argv, "b:n:o:v:ct")) != EOF) {
		switch (fd) {
		case 'b':
			blksize = atoi(optarg) * 512;
			break;
		case 'n':
			maxblks = atoll(optarg);
			break;
		case 'o':
			offset = atoll(optarg);
			break;
		case 'v':
			value = atoi(optarg);
			break;
		case 'c':
			oflags |= O_CREAT;
			break;
		case 't':
			oflags |= O_TRUNC;
			break;
		default:
			sts++;
		}
	}
	if (sts || argc - optind != 1) {
		fprintf(stderr,
			"Usage: %s [-b N*512] [-n N] [-o off] [-v val] "
				" [-c] [-t] <dev/file>\n",
			progname);
		fprintf(stderr,"      -c: create -t: truncate\n");
		exit(1);
	}

	path = argv[optind];

	if ((fd = open(path, oflags)) < 0) {
		fprintf(stderr,
			"error opening \"%s\": %s\n",
			path, strerror(errno));
		exit(1);
	}
	if ((lseek64(fd, offset, SEEK_SET)) < 0) {
		fprintf(stderr, "%s: error seeking to offset %llu "
					"on \"%s\": %s\n",
			progname, offset, path, strerror(errno));
		exit(1);
	}

	if ((z = memalign(getpagesize(), blksize)) == NULL) {
		fprintf(stderr, "%s: can't memalign %u bytes: %s\n",
			progname, blksize, strerror(errno));
		exit(1);
	}
	memset(z, value, blksize);
	errno = 0;
	for (;;) {
		if (nblks++ == maxblks)
			break;
		if ((sts = write(fd, z, blksize)) < blksize) {
			if (errno == ENOSPC || sts >= 0)
				break;
			fprintf(stderr, "%s: write failed: %s\n",
				progname, strerror(errno));
			break;
		}
	}
	printf("Wrote %.2fKb (value 0x%x)\n",
		(double) ((--nblks) * blksize) / 1024, value);
	free(z);
	return 0;
}
