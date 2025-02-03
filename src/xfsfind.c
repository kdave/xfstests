// SPDX-License-Identifier: GPL-2.0
/*
 * find(1) but with special predicates for finding XFS attributes.
 * Copyright (C) 2022 Oracle.
 */
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <ftw.h>
#include <linux/fs.h>

#include "global.h"

static int want_anyfile;
static int want_datafile;
static int want_attrfile;
static int want_dir;
static int want_regfile;
static int want_sharedfile;
static int report_errors = 1;
static int print0;

static int
check_datafile(
	const char		*path,
	int			fd)
{
	off_t			off;

	off = lseek(fd, 0, SEEK_DATA);
	if (off >= 0)
		return 1;

	if (errno == ENXIO)
		return 0;

	if (report_errors)
		perror(path);

	return -1;
}

static int
check_attrfile(
	const char		*path,
	int			fd)
{
	struct fsxattr		fsx;
	int			ret;

	ret = ioctl(fd, XFS_IOC_FSGETXATTR, &fsx);
	if (ret) {
		if (report_errors)
			perror(path);
		return -1;
	}

	if (want_attrfile && (fsx.fsx_xflags & XFS_XFLAG_HASATTR))
		return 1;
	return 0;
}

#define BMAP_NR			33
static struct getbmapx		bmaps[BMAP_NR];

static int
check_sharedfile(
	const char		*path,
	int			fd)
{
	struct getbmapx		*key = &bmaps[0];
	unsigned int		i;
	int			ret;

	memset(key, 0, sizeof(struct getbmapx));
	key->bmv_length = ULLONG_MAX;
	/* no holes and don't flush dirty pages */
	key->bmv_iflags = BMV_IF_DELALLOC | BMV_IF_NO_HOLES;
	key->bmv_count = BMAP_NR;

	while ((ret = ioctl(fd, XFS_IOC_GETBMAPX, bmaps)) == 0) {
		struct getbmapx	*p = &bmaps[1];
		xfs_off_t	new_off;

		for (i = 0; i < key->bmv_entries; i++, p++) {
			if (p->bmv_oflags & BMV_OF_SHARED)
				return 1;
		}

		if (key->bmv_entries == 0)
			break;
		p = key + key->bmv_entries;
		if (p->bmv_oflags & BMV_OF_LAST)
			return 0;

		new_off = p->bmv_offset + p->bmv_length;
		key->bmv_length -= new_off - key->bmv_offset;
		key->bmv_offset = new_off;
	}
	if (ret < 0) {
		if (report_errors)
			perror(path);
		return -1;
	}

	return 0;
}

static void
print_help(
	const char		*name)
{
	printf("Usage: %s [OPTIONS] path\n", name);
	printf("\n");
	printf("Print all file paths matching any of the given predicates.\n");
	printf("\n");
	printf("-0	Print nulls between paths instead of newlines.\n");
	printf("-a	Match files with xattrs.\n");
	printf("-b	Match files with data blocks.\n");
	printf("-d	Match directories.\n");
	printf("-q	Ignore errors while walking directory tree.\n");
	printf("-r	Match regular files.\n");
	printf("-s	Match files with shared blocks.\n");
	printf("\n");
	printf("If no matching options are given, match all files found.\n");
}

static int
visit(
	const char		*path,
	const struct stat	*sb,
	int			typeflag,
	struct FTW		*ftwbuf)
{
	int			printme = 1;
	int			fd = -1;
	int			retval = FTW_CONTINUE;

	if (want_anyfile)
		goto out;
	if (want_regfile && typeflag == FTW_F)
		goto out;
	if (want_dir && typeflag == FTW_D)
		goto out;

	/*
	 * We can only open directories and files; screen out everything else.
	 * Note that nftw lies and reports FTW_F for device files, so check the
	 * statbuf mode too.
	 */
	if (typeflag != FTW_F && typeflag != FTW_D) {
		printme = 0;
		goto out;
	}

	if (!S_ISREG(sb->st_mode) && !S_ISDIR(sb->st_mode)) {
		printme = 0;
		goto out;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (report_errors) {
			perror(path);
			return FTW_STOP;
		}

		return FTW_CONTINUE;
	}

	if (want_datafile && typeflag == FTW_F) {
		int ret = check_datafile(path, fd);
		if (ret < 0 && report_errors) {
			printme = 0;
			retval = FTW_STOP;
			goto out_fd;
		}

		if (ret == 1)
			goto out_fd;
	}

	if (want_attrfile) {
		int ret = check_attrfile(path, fd);
		if (ret < 0 && report_errors) {
			printme = 0;
			retval = FTW_STOP;
			goto out_fd;
		}

		if (ret == 1)
			goto out_fd;
	}

	if (want_sharedfile) {
		int ret = check_sharedfile(path, fd);
		if (ret < 0 && report_errors) {
			printme = 0;
			retval = FTW_STOP;
			goto out_fd;
		}

		if (ret == 1)
			goto out_fd;
	}

	printme = 0;
out_fd:
	close(fd);
out:
	if (printme) {
		if (print0)
			printf("%s%c", path, 0);
		else
			printf("%s\n", path);
		fflush(stdout);
	}
	return retval;
}

static void
handle_sigabrt(
	int		signal,
	siginfo_t	*info,
	void		*ucontext)
{
	fprintf(stderr, "Signal %u, exiting.\n", signal);
	exit(2);
}

int
main(
	int			argc,
	char			*argv[])
{
	struct rlimit		rlimit;
	struct sigaction	abrt = {
		.sa_sigaction	= handle_sigabrt,
		.sa_flags	= SA_SIGINFO,
	};
	int			c;
	int			ret;

	while ((c = getopt(argc, argv, "0abdqrs")) >= 0) {
		switch (c) {
		case '0':	print0 = 1;          break;
		case 'a':	want_attrfile = 1;   break;
		case 'b':	want_datafile = 1;   break;
		case 'd':	want_dir = 1;        break;
		case 'q':	report_errors = 0;   break;
		case 'r':	want_regfile = 1;    break;
		case 's':	want_sharedfile = 1; break;
		default:
			print_help(argv[0]);
			return 1;
		}
	}

	ret = getrlimit(RLIMIT_NOFILE, &rlimit);
	if (ret) {
		perror("RLIMIT_NOFILE");
		return 1;
	}

	if (!want_attrfile && !want_datafile && !want_dir && !want_regfile &&
	    !want_sharedfile)
		want_anyfile = 1;

	/*
	 * nftw is known to abort() if a directory it is walking disappears out
	 * from under it.  Handle this with grace if the caller wants us to run
	 * quietly.
	 */
	if (!report_errors) {
		ret = sigaction(SIGABRT, &abrt, NULL);
		if (ret) {
			perror("SIGABRT handler");
			return 1;
		}
	}

	for (c = optind; c < argc; c++) {
		ret = nftw(argv[c], visit, rlimit.rlim_cur - 5,
				FTW_ACTIONRETVAL | FTW_CHDIR | FTW_MOUNT |
				FTW_PHYS);
		if (ret && report_errors) {
			perror(argv[c]);
			break;
		}
	}

	if (ret)
		return 1;
	return 0;
}
