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
 * Test for filesystem features on given mount point or device
 *   -q  test for quota support (kernel compile option)
 *   -u  test for user quota enforcement support (mount option)
 *   -g  test for group quota enforcement support (mount option)
 *   -U  test for user quota accounting support (mount option)
 *   -G  test for group quota accounting support (mount option)
 * Return code: 0 is true, anything else is error/not supported
 */

#include <libxfs.h>
#include <sys/quota.h>
#include <xqm.h>

int verbose = 0;

void
usage(void)
{
	fprintf(stderr, "Usage: feature [-v] -<q|u|g|U|G> <filesystem>\n");
	exit(1);
}

int
hasxfsquota(int type, int q, char *device)
{
	fs_quota_stat_t	qstat;
	int		qcmd;

	memset(&qstat, 0, sizeof(fs_quota_stat_t));
	qcmd = QCMD(Q_XGETQSTAT, type);
	if (quotactl(qcmd, device, 0, (caddr_t)&qstat) < 0) {
		if (verbose)
			perror("quotactl");
		return (1);
	}
	else if (q == 0)
		return (0);
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
	int	gflag = 0;
	int	Gflag = 0;
	int	qflag = 0;
	int	uflag = 0;
	int	Uflag = 0;
	char	*fs;

	while ((c = getopt(argc, argv, "gGquUv")) != EOF) {
		switch (c) {
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

	if (!uflag && !gflag && !qflag && !Uflag && !Gflag)
		usage();
	if (optind != argc-1)
		usage();
	fs = argv[argc-1];

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
