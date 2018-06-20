// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2004 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#include <syslog.h>
#include "global.h"

static char *xprogname;


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-f] [-v] mnt-dir\n", xprogname);
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

      	xprogname = argv[0];

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
			xprogname, mnt_dir, strerror(errno));
		return 1;
	}

	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "%s: argument \"%s\" is not a directory\n",
			xprogname, mnt_dir);
		return 1;
	}

	
#if 0
	{
		struct statvfs stvfs;
		if ((statvfs(mnt_dir, &stvfs)) == -1) {
			fprintf(stderr, "%s: error on statfs \"%s\": %s\n",
				xprogname, mnt_dir, strerror(errno));
			return 1;
		}

		if (strcmp(stvfs.f_basetype, "xfs") != 0) {
			fprintf(stderr, "%s: filesys for \"%s\" is not XFS:\"%s\"\n",
				xprogname, mnt_dir, stvfs.f_basetype);
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
			xprogname, mnt_dir, strerror(errno));
		return 1;
	}

	if (verbose_opt) {
		printf("Calling XFS_IOC_GOINGDOWN\n");
	}
	syslog(LOG_WARNING, "xfstests-induced forced shutdown of %s:\n",
		mnt_dir);
	if ((xfsctl(mnt_dir, fd, XFS_IOC_GOINGDOWN, &flag)) == -1) {
		fprintf(stderr, "%s: error on xfsctl(GOINGDOWN) of \"%s\": %s\n",
			xprogname, mnt_dir, strerror(errno));
		return 1;
	}

	close(fd);

	return 0;
}
