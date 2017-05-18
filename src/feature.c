/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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

/*
 * Test for filesystem features on given mount point or device
 *   -c  test for 32bit chown support (via libc)
 *   -t  test for working rlimit/ftruncate64 (via libc)
 *   -q  test for quota support (kernel compile option)
 *   -u  test for user quota enforcement support (mount option)
 *   -p  test for project quota enforcement support (mount option)
 *   -g  test for group quota enforcement support (mount option)
 *   -U  test for user quota accounting support (mount option)
 *   -G  test for group quota accounting support (mount option)
 *   -P  test for project quota accounting support (mount option)
 * Return code: 0 is true, anything else is error/not supported
 *
 * Test for machine features
 *   -A  test whether AIO syscalls are available
 *   -o  report a number of online cpus
 *   -s  report pagesize
 *   -w  report bits per long
 */

#include "global.h"

#include <sys/quota.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_XFS_XQM_H
#include <xfs/xqm.h>
#endif

#ifdef HAVE_LIBAIO_H
#include <libaio.h>
#endif

#ifndef USRQUOTA
#define USRQUOTA  0
#endif

#ifndef GRPQUOTA
#define GRPQUOTA  1
#endif

#ifndef PRJQUOTA
#define PRJQUOTA  2
#endif

int verbose = 0;

void
usage(void)
{
	fprintf(stderr, "Usage: feature [-v] -<q|u|g|p|U|G|P> <filesystem>\n");
	fprintf(stderr, "       feature [-v] -c <file>\n");
	fprintf(stderr, "       feature [-v] -t <file>\n");
	fprintf(stderr, "       feature -A | -o | -s | -w\n");
	exit(1);
}

int check_big_ID(char *filename)
{
	struct stat64	sbuf;

	memset(&sbuf, 0, sizeof(struct stat64));
	if (lstat64(filename, &sbuf) < 0) {
		fprintf(stderr, "lstat64 failed on ");
		perror(filename);
		return(1);
	}

	/* 98789 is greater than 2^16 (65536) */
	if ((__uint32_t)sbuf.st_uid == 98789 || (__uint32_t)sbuf.st_gid == 98789)
		return(0);
	if (verbose)
		fprintf(stderr, "lstat64 on %s gave uid=%d, gid=%d\n",
			filename, (int)sbuf.st_uid, (int)sbuf.st_gid);
	return(1);
}

int
haschown32(char *filename)
{
	if (check_big_ID(filename) == 0)
		return(0);

	if (chown(filename, 98789, 98789) < 0) {
		fprintf(stderr, "chown failed on ");
		perror(filename);
		return(1);
	}

	if (check_big_ID(filename) == 0)
		return(0);
	return (1);
}

int
hastruncate64(char *filename)
{
	struct rlimit64 rlimit64;
	off64_t bigoff = 4294967307LL;	/* > 2^32 */
	struct stat64 bigst;
	int fd;

	getrlimit64(RLIMIT_FSIZE, &rlimit64);
	rlimit64.rlim_cur = RLIM64_INFINITY;
	setrlimit64(RLIMIT_FSIZE, &rlimit64);

	signal(SIGXFSZ, SIG_IGN);

	if ((fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
		fprintf(stderr, "open failed on ");
		perror(filename);
		return(1);
	}

	if (ftruncate64(fd, bigoff) < 0)
		return(1);

	if (fstat64(fd, &bigst) < 0) {
		fprintf(stderr, "fstat64 failed on ");
		perror(filename);
		return(1);
	}

	if (verbose)
		fprintf(stderr, "fstat64 on %s gave sz=%lld (truncsz=%lld)\n",
			filename, (long long)bigst.st_size, (long long)bigoff);

	if (bigst.st_size != bigoff)
		return(1);
	return(0);
}

int
hasxfsquota(int type, int q, char *device)
{
	fs_quota_stat_t	qstat;
	int		qcmd;

	memset(&qstat, 0, sizeof(fs_quota_stat_t));

#ifdef QCMD
	if (q == 0) {
		if (access("/proc/fs/xfs/xqm", F_OK) < 0) {
			if (verbose) {
				fprintf(stderr, "can't access /proc/fs/xfs/xqm\n");
			}
			return 1;
		}
		return 0;
	}
	qcmd = QCMD(Q_XGETQSTAT, type);
#else
	if (q == 0) {
		if (quotactl(Q_SYNC, device, 0, (caddr_t)&qstat) == ENOPKG) {
			if (verbose) {
				fprintf(stderr, "Q_SYNC not supported\n");
			}
			return 1;
		}
		return 0;
	}
	qcmd = Q_GETQSTAT;
#endif

	if (quotactl(qcmd, device, 0, (caddr_t)&qstat) < 0) {
		if (verbose)
			perror("quotactl");
		return (1);
	}
	else if (q == XFS_QUOTA_UDQ_ENFD && qstat.qs_flags & XFS_QUOTA_UDQ_ENFD)
		return (0);
	else if (q == XFS_QUOTA_GDQ_ENFD && qstat.qs_flags & XFS_QUOTA_GDQ_ENFD)
		return (0);
	else if (q == XFS_QUOTA_PDQ_ENFD && qstat.qs_flags & XFS_QUOTA_PDQ_ENFD)
		return (0);
	else if (q == XFS_QUOTA_UDQ_ACCT && qstat.qs_flags & XFS_QUOTA_UDQ_ACCT)
		return (0);
	else if (q == XFS_QUOTA_GDQ_ACCT && qstat.qs_flags & XFS_QUOTA_GDQ_ACCT)
		return (0);
	else if (q == XFS_QUOTA_PDQ_ACCT && qstat.qs_flags & XFS_QUOTA_PDQ_ACCT)
		return (0);
	if (verbose)
		fprintf(stderr, "quota type (%d) not available\n", q);
	return (1);
}

static int
check_aio_support(void)
{
#ifdef HAVE_LIBAIO_H
	struct io_context *ctx = NULL;
	int err;

	err = io_setup(1, &ctx);
	if (err == 0)
		return 0;

	if (err == -ENOSYS) /* CONFIG_AIO=n */
		return 1;

	fprintf(stderr, "unexpected error from io_setup(): %s\n",
		strerror(-err));
	return 2;
#else
	/* libaio was unavailable at build time; assume AIO is unsupported */
	return 1;
#endif
}


int
main(int argc, char **argv)
{
	int	c;
	int	Aflag = 0;
	int	cflag = 0;
	int	tflag = 0;
	int	gflag = 0;
	int	Gflag = 0;
	int	pflag = 0;
	int	Pflag = 0;
	int	qflag = 0;
	int	sflag = 0;
	int	uflag = 0;
	int	Uflag = 0;
	int	wflag = 0;
	int	oflag = 0;
	char	*fs = NULL;

	while ((c = getopt(argc, argv, "ActgGopPqsuUvw")) != EOF) {
		switch (c) {
		case 'A':
			Aflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'g':
			gflag++;
			break;
		case 'G':
			Gflag++;
			break;
		case 'o':
			oflag++;
			break;
		case 'p':
			pflag++;
			break;
		case 'P':
			Pflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'U':
			Uflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	/* filesystem features */
	if (cflag|tflag|uflag|gflag|pflag|qflag|Uflag|Gflag|Pflag) {
		if (optind != argc-1)	/* need a device */
			usage();
		fs = argv[argc-1];
	} else if (Aflag || wflag || sflag || oflag) {
		if (optind != argc)
			usage();
	} else 
		usage();

	if (cflag)
		return(haschown32(fs));
	if (tflag)
		return(hastruncate64(fs));
	if (qflag)
		return(hasxfsquota(0, 0, fs));
	if (gflag)
		return(hasxfsquota(GRPQUOTA, XFS_QUOTA_GDQ_ENFD, fs));
	if (pflag)
		return(hasxfsquota(PRJQUOTA, XFS_QUOTA_PDQ_ENFD, fs));
	if (uflag)
		return(hasxfsquota(USRQUOTA, XFS_QUOTA_UDQ_ENFD, fs));
	if (Gflag)
		return(hasxfsquota(GRPQUOTA, XFS_QUOTA_GDQ_ACCT, fs));
	if (Pflag)
		return(hasxfsquota(PRJQUOTA, XFS_QUOTA_PDQ_ACCT, fs));
	if (Uflag)
		return(hasxfsquota(USRQUOTA, XFS_QUOTA_UDQ_ACCT, fs));

	if (Aflag)
		return(check_aio_support());

	if (sflag) {
		printf("%d\n", getpagesize());
		exit(0);
	}
	if (wflag) {
#ifdef BITS_PER_LONG
		printf("%d\n", BITS_PER_LONG);
#else
#ifdef NBBY
		/* This can change under IRIX depending on whether we compile
		 * with -n32/-32 or -64
		 */
		printf("%d\n", (int)(NBBY * sizeof(long)));
#else
bozo!
#endif
#endif
		exit(0);
	}
	if (oflag) {
		long ncpus = -1;

#if defined(_SC_NPROCESSORS_ONLN)
		ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
		ncpus = sysconf(_SC_NPROC_ONLN);
#endif
		if (ncpus == -1)
			ncpus = 1;

		printf("%ld\n", ncpus);

		exit(0);
	}

	fprintf(stderr, "feature: dunno what you're after.\n");
	return(1);
}
