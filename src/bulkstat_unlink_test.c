/*
 * $Id: bulkstat_unlink_test.c,v 1.3 2007/10/30 03:07:42 mohamedb.longdrop.melbourne.sgi.com Exp $
 * Test bulkstat doesn't returned unlinked inodes.
 * Mark Goodwin <markgw@sgi.com> Fri Jul 20 09:13:57 EST 2007
 */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xfs/xfs.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	int e;
	int i;
	int j;
	int k;
	int nfiles;
	int stride;

	int c;

	struct stat sbuf;
	ino_t *inodelist;
	xfs_fsop_bulkreq_t a;
	xfs_bstat_t *ret;
	int iterations;
	char fname[MAXPATHLEN];
	char *dirname;
	int chknb = 0;

	while ((c = getopt(argc, argv, "r")) != -1) {
		switch(c) {
		case 'r':
			chknb = 1;
			break;
		default:
			break;
		}
	}

	if ((argc - optind) != 4) {
		fprintf(stderr, "Usage: %s iterations nfiles stride dir [options]\n", argv[0]);
		fprintf(stderr, "Create dir with nfiles, unlink each stride'th file, sync, bulkstat\n");
		exit(1);
	}


	iterations = atoi(argv[optind++]);
	nfiles     = atoi(argv[optind++]);
	stride     = atoi(argv[optind++]);

	dirname = argv[optind++];

	if (chknb)
		printf("Runing extended checks.\n");

	inodelist = (ino_t *)malloc(nfiles * sizeof(ino_t));
	ret = (xfs_bstat_t *)malloc(nfiles * sizeof(xfs_bstat_t));

	for (k=0; k < iterations; k++) {
		int fd[nfiles + 1];
		xfs_ino_t last_inode = 0;
		int count = 0, scount = -1;

		printf("Iteration %d ... (%d files)", k, nfiles);

		memset(&a, 0, sizeof(xfs_fsop_bulkreq_t));
		a.lastip = (__u64 *)&last_inode;
		a.icount = nfiles;
		a.ubuffer = ret;
		a.ocount = &count;

		if (mkdir(dirname, 0755) < 0) {
			printf("Warning (%s,%d), mkdir(%s) failed.\n", __FILE__, __LINE__, dirname);
			perror(dirname);
			exit(1);
		}

		if ((fd[nfiles] = open(dirname, O_RDONLY)) < 0) {
			printf("Warning (%s,%d), open(%s) failed.\n", __FILE__, __LINE__, dirname);
			perror(dirname);
			exit(1);
		}

		if (chknb) { /* Get the original number of inodes (lazy) */
			sync();
			if (xfsctl(dirname, fd[nfiles], XFS_IOC_FSBULKSTAT, &a) != 0) {
				printf("Warning (%s:%d), xfsctl(XFS_IOC_FSBULKSTAT) FAILED.\n", __FILE__, __LINE__);
			}

			scount = count;
		}

		for (i=0; i < nfiles; i++) { /* Open the files */
			sprintf(fname, "%s/file%06d", dirname, i);
			if ((fd[i] = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
				printf("Warning (%s,%d), open(%s) failed.\n", __FILE__, __LINE__, fname);
				perror(fname);
				exit(1);
			}
			write(fd[i], fname, sizeof(fname));
			if (fstat(fd[i], &sbuf) < 0) {
				printf("Warning (%s,%d), fstat failed.\n", __FILE__, __LINE__);
				perror(fname);
				exit(1);
			}
			inodelist[i] = sbuf.st_ino;
			unlink(fname);
		}

		if (chknb) {
			/*
			 *The files are still opened (but unlink()ed) ,
			 * we should have more inodes than before
			 */
			sync();
			last_inode = 0;
			if (xfsctl(dirname, fd[nfiles], XFS_IOC_FSBULKSTAT, &a) != 0) {
				printf("Warning (%s:%d), xfsctl(XFS_IOC_FSBULKSTAT) FAILED.\n", __FILE__, __LINE__);
			}
			if (count < scount) {
				printf("ERROR, count(%d) < scount(%d).\n", count, scount);
				return -1;
			}
		}

		/* Close all the files */
		for (i = 0; i < nfiles; i++) {
			close(fd[i]);
		}

		if (chknb) {
			/*
			 * The files are now closed, we should be back to our,
			 * previous inode count
			 */
			sync();
			last_inode = 0;
			if (xfsctl(dirname, fd[nfiles], XFS_IOC_FSBULKSTAT, &a) != 0) {
				printf("Warning (%s:%d), xfsctl(XFS_IOC_FSBULKSTAT) FAILED.\n", __FILE__, __LINE__);
			}
			if (count != scount) {
				printf("ERROR, count(%d) != scount(%d).\n", count, scount);
				return -1;
			}
		}

		sync();
		last_inode = 0;
		for (;;) {
			if ((e = xfsctl(dirname, fd[nfiles], XFS_IOC_FSBULKSTAT, &a)) < 0) {
				printf("Warning (%s,%d), xfsctl failed.\n", __FILE__, __LINE__);
				perror("XFS_IOC_FSBULKSTAT:");
				exit(1);
			}

			if (count == 0)
				break;

			for (i=0; i < count; i++) {
				for (j=0; j < nfiles; j += stride) {
					if (ret[i].bs_ino == inodelist[j]) {
						/* oops ... */
						printf("failed. Unlinked inode %llu returned by bulkstat\n", (unsigned long long)inodelist[j]);
						exit(1);
					}
				}
			}
		}

		close(fd[nfiles]);
		sprintf(fname, "rm -rf %s\n", dirname);
		system(fname);

		sync();
		sleep(2);
		printf("passed\n");
	}

	exit(0);
}
