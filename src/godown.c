/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "global.h"

/* These should be in libxfs.h */
#ifndef XFS_IOC_GOINGDOWN
#define XFS_IOC_GOINGDOWN           _IOR ('X', 125, __uint32_t)
#endif
#ifndef XFS_FSOP_GOING_FLAGS_DEFAULT
#define XFS_FSOP_GOING_FLAGS_DEFAULT    0x0     /* going down */
#endif
#ifndef XFS_FSOP_GOING_FLAGS_LOGFLUSH
#define XFS_FSOP_GOING_FLAGS_LOGFLUSH    0x1     /* flush log */
#endif
#ifndef XFS_FSOP_GOING_FLAGS_NOLOGFLUSH
#define XFS_FSOP_GOING_FLAGS_NOLOGFLUSH  0x2     /* don't flush log */
#endif

static char *progname;


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-f] [-v] mnt-dir\n", progname);
}

int
main(int argc, char *argv[])
{
	int c;
	int flag;
	int flushlog_opt = 0;
	int verbose_opt = 0;
	struct stat st;
	char *mnt_dir;
	int fd;

      	progname = argv[0];

	while ((c = getopt(argc, argv, "fv")) != -1) {
		switch (c) {
		case 'f':
			flushlog_opt = 1;
			break;
		case 'v':
			verbose_opt = 1;
			break;
		case '?':
			usage();
			return 1;
		}
	}

	/* process required cmd argument */
	if (optind == argc-1) {
		mnt_dir = argv[optind];
	}
	else {
		usage();
		return 1;
	}

	if ((stat(mnt_dir, &st)) == -1) {
		fprintf(stderr, "%s: error on stat \"%s\": %s\n",
			progname, mnt_dir, strerror(errno));
		return 1;
	}

	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "%s: argument \"%s\" is not a directory\n",
			progname, mnt_dir);
		return 1;
	}

	
#if 0
	{
		struct statvfs stvfs;
		if ((statvfs(mnt_dir, &stvfs)) == -1) {
			fprintf(stderr, "%s: error on statfs \"%s\": %s\n",
				progname, mnt_dir, strerror(errno));
			return 1;
		}

		if (strcmp(stvfs.f_basetype, "xfs") != 0) {
			fprintf(stderr, "%s: filesys for \"%s\" is not XFS:\"%s\"\n",
				progname, mnt_dir, stvfs.f_basetype);
			return 1;
		}
	}
#endif


	flag = (flushlog_opt ? XFS_FSOP_GOING_FLAGS_LOGFLUSH 
			    : XFS_FSOP_GOING_FLAGS_NOLOGFLUSH);

	if (verbose_opt) {
		printf("Opening \"%s\"\n", mnt_dir);
	}
	if ((fd = open(mnt_dir, O_RDONLY)) == -1) {
		fprintf(stderr, "%s: error on open of \"%s\": %s\n",
			progname, mnt_dir, strerror(errno));
		return 1;
	}

	if (verbose_opt) {
		printf("Calling XFS_IOC_GOINGDOWN\n");
	}
	if ((xfsctl(mnt_dir, fd, XFS_IOC_GOINGDOWN, &flag)) == -1) {
		fprintf(stderr, "%s: error on xfsctl(GOINGDOWN) of \"%s\": %s\n",
			progname, mnt_dir, strerror(errno));
		return 1;
	}

	close(fd);

	return 0;
}
