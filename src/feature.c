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

/*
 * Test for filesystem features on given mount point or device
 *   -c  test for 32bit chown support (via libc)
 *   -t  test for working rlimit/ftruncate64 (via libc)
 *   -q  test for quota support (kernel compile option)
 *   -u  test for user quota enforcement support (mount option)
 *   -g  test for group quota enforcement support (mount option)
 *   -U  test for user quota accounting support (mount option)
 *   -G  test for group quota accounting support (mount option)
 * Return code: 0 is true, anything else is error/not supported
 */

#include <xfs/libxfs.h>
#include <sys/quota.h>
#include <sys/resource.h>
#include <signal.h>
#include <xfs/xqm.h>

int verbose = 0;

void
usage(void)
{
	fprintf(stderr, "Usage: feature [-v] -<q|u|g|U|G> <filesystem>\n");
	fprintf(stderr, "       feature [-v] -c <file>\n");
	fprintf(stderr, "       feature [-v] -t <file>\n");
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
	if ((__u32)sbuf.st_uid == 98789 || (__u32)sbuf.st_gid == 98789)
		return(0);
	if (verbose)
		fprintf(stderr, "lstat64 on %s gave uid=%d, gid=%d\n",
			filename, sbuf.st_uid, sbuf.st_gid);
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
	off64_t	bigoff = 4294967307;	/* > 2^32 */
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
			filename, bigst.st_size, bigoff);

	if (bigst.st_size != bigoff)
		return(1);
	return(0);
}

int
hasxfsquota(int type, int q, char *device)
{
	fs_quota_stat_t	qstat;
	int		qcmd;

	if (q == 0)
		return (access("/proc/fs/xfs/xqm", F_OK) < 0);

	memset(&qstat, 0, sizeof(fs_quota_stat_t));
	qcmd = QCMD(Q_XGETQSTAT, type);
	if (quotactl(qcmd, device, 0, (caddr_t)&qstat) < 0) {
		if (verbose)
			perror("quotactl");
		return (1);
	}
	else if (q == XFS_QUOTA_UDQ_ENFD && qstat.qs_flags & XFS_QUOTA_UDQ_ENFD)
		return (0);
	else if (q == XFS_QUOTA_GDQ_ENFD && qstat.qs_flags & XFS_QUOTA_GDQ_ENFD)
		return (0);
	else if (q == XFS_QUOTA_UDQ_ACCT && qstat.qs_flags & XFS_QUOTA_UDQ_ACCT)
		return (0);
	else if (q == XFS_QUOTA_GDQ_ACCT && qstat.qs_flags & XFS_QUOTA_GDQ_ACCT)
		return (0);
	if (verbose)
		fprintf(stderr, "quota type (%d) not available\n", q);
	return (1);
}

int
main(int argc, char **argv)
{
	int	c;
	int	cflag = 0;
	int	tflag = 0;
	int	gflag = 0;
	int	Gflag = 0;
	int	qflag = 0;
	int	uflag = 0;
	int	Uflag = 0;
	char	*fs;

	while ((c = getopt(argc, argv, "ctgGquUv")) != EOF) {
		switch (c) {
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
		case 'q':
			qflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'U':
			Uflag++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	if (!cflag && !tflag && !uflag && !gflag && !qflag && !Uflag && !Gflag)
		usage();
	if (optind != argc-1)
		usage();
	fs = argv[argc-1];

	if (cflag)
		return(haschown32(fs));
	if (tflag)
		return(hastruncate64(fs));
	if (qflag)
		return(hasxfsquota(0, 0, fs));
	if (gflag)
		return(hasxfsquota(GRPQUOTA, XFS_QUOTA_GDQ_ENFD, fs));
	if (uflag)
		return(hasxfsquota(USRQUOTA, XFS_QUOTA_UDQ_ENFD, fs));
	if (Gflag)
		return(hasxfsquota(GRPQUOTA, XFS_QUOTA_GDQ_ACCT, fs));
	if (Uflag)
		return(hasxfsquota(USRQUOTA, XFS_QUOTA_UDQ_ACCT, fs));

	fprintf(stderr, "feature: dunno what you're doing?\n");
	return(1);
}
