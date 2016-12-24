/*
 * Copyright (c) 2016 Red Hat, Inc.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/quota.h>
#include <sys/types.h>
#include <xfs/xqm.h>

/*
 * Exercise the Q_GETNEXTQUOTA and Q_XGETNEXTQUOTA quotactls.
 * Really only returns a bare minimum of quota information,
 * just enough to be sure we got a sane answer back.
 *
 * These quotactls take a quota ID as input, and return the
 * next active quota >= that ID.
 *
 * usage:
 * 	test-nextquota [-v] -[u|g|p] -i id -d device
 */

#ifndef PRJQUOTA
#define PRJQUOTA 2
#endif

#ifndef Q_GETNEXTQUOTA
#define Q_GETNEXTQUOTA 0x800009        /* get disk limits and usage >= ID */
#endif

/* glibc 2.24 defines Q_GETNEXTQUOTA but not struct nextdqblk. */
struct test_nextdqblk
  {
    u_int64_t dqb_bhardlimit;	/* absolute limit on disk quota blocks alloc */
    u_int64_t dqb_bsoftlimit;	/* preferred limit on disk quota blocks */
    u_int64_t dqb_curspace;	/* current quota block count */
    u_int64_t dqb_ihardlimit;	/* maximum # allocated inodes */
    u_int64_t dqb_isoftlimit;	/* preferred inode limit */
    u_int64_t dqb_curinodes;	/* current # allocated inodes */
    u_int64_t dqb_btime;	/* time limit for excessive disk use */
    u_int64_t dqb_itime;	/* time limit for excessive files */
    u_int32_t dqb_valid;	/* bitmask of QIF_* constants */
    u_int32_t dqb_id;		/* id for this quota info*/
  };

#ifndef Q_XGETNEXTQUOTA
#define Q_XGETNEXTQUOTA XQM_CMD(9)
#endif

void usage(char *progname)
{
	printf("usage: %s [-v] -[u|g|p] -i id -d device\n", progname);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int cmd;
	int type = -1, typeflag = 0;
	int verbose = 0;
	int retval = 0;
	uint id = 0, idflag = 0;
	char *device = NULL;
	char *tmp;
	struct test_nextdqblk dqb;
	struct fs_disk_quota xqb;

	while ((c = getopt(argc,argv,"ugpi:d:v")) != EOF) {
		switch (c) {
		case 'u':
			type = USRQUOTA;
			typeflag++;
			break;
		case 'g':
			type = GRPQUOTA;
			typeflag++;
			break;
		case 'p':
			type = PRJQUOTA;
			typeflag++;
			break;
		case 'i':
			id = (uint) strtoul(optarg, &tmp, 0);
			if (*tmp) {
				fprintf(stderr, "Bad id: %s\n", optarg);
				exit(1);
			}
			idflag++;
			break;
		case 'd':
			device = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (idflag == 0) {
		printf("No id specified\n");
		usage(argv[0]);
	}
	if (typeflag == 0) {
		printf("No type specified\n");
		usage(argv[0]);
	}
	if (typeflag > 1) {
		printf("Multiple types specified\n");
		usage(argv[0]);
	}
	if (device == NULL) {
		printf("No device specified\n");
		usage(argv[0]);
	}

	if (verbose)
		printf("asking for quota type %d for id %u on %s\n", type, id, device);

	memset(&dqb, 0, sizeof(struct test_nextdqblk));
	memset(&xqb, 0, sizeof(struct fs_disk_quota));

	if (verbose)
		printf("====Q_GETNEXTQUOTA====\n");
	cmd = QCMD(Q_GETNEXTQUOTA, type);
	if (quotactl(cmd, device, id, (void *)&dqb) < 0) {
		perror("Q_GETNEXTQUOTA");
		retval = 1;
	} else {
		/*
		 * We only print id and inode limits because
		 * block count varies depending on fs block size, etc;
		 * this is just a sanity test that we can retrieve the quota,
		 * and inode limits have the same units across both calls.
		 */
		printf("id        %u\n", dqb.dqb_id);
		printf("ihard     %llu\n",
				  (unsigned long long)dqb.dqb_ihardlimit);
		printf("isoft     %llu\n",
				  (unsigned long long)dqb.dqb_isoftlimit);
	}

	if (verbose)
		printf("====Q_XGETNEXTQUOTA====\n");
	cmd = QCMD(Q_XGETNEXTQUOTA, USRQUOTA);
	if (quotactl(cmd, device, id, (void *)&xqb) < 0) {
		perror("Q_XGETNEXTQUOTA");
		retval = 1;
	} else {
		printf("id        %u\n", xqb.d_id);
		printf("ihard     %llu\n", xqb.d_ino_hardlimit);
		printf("isoft     %llu\n", xqb.d_ino_softlimit);
	}

	return retval;
}
