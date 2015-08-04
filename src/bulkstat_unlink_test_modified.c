/*
 * $Id: bulkstat_unlink_test_modified.c,v 1.1 2007/10/03 16:23:57 mohamedb.longdrop.melbourne.sgi.com Exp $
 * Test bulkstat doesn't returned unlinked inodes.
 * Mark Goodwin <markgw@sgi.com> Fri Jul 20 09:13:57 EST 2007
 *
 * This is a modified version of bulkstat_unlink_test.c to reproduce a specific
 * problem see pv 969192
 */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xfs/xfs.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char *argv[])
{
    int e;
    int fd = 0;
    int i;
    int j;
    int k;
    int nfiles;
    int stride;
    struct stat sbuf;
    ino_t *inodelist;
    __u32 *genlist;
    xfs_fsop_bulkreq_t a;
    xfs_bstat_t *ret;
    int iterations;
    char fname[MAXPATHLEN];
    char *dirname;

    if (argc != 5) {
    	fprintf(stderr, "Usage: %s iterations nfiles stride dir\n", argv[0]);
    	fprintf(stderr, "Create dir with nfiles, unlink each stride'th file, sync, bulkstat\n");
	exit(1);
    }

    iterations = atoi(argv[1]);
    nfiles = atoi(argv[2]);
    stride = atoi(argv[3]);
    dirname = argv[4];
    if (!nfiles || !iterations) {
	fprintf(stderr, "Iterations and nfiles showld be non zero.\n");
    	exit(1);
    }

    inodelist = (ino_t *)malloc(nfiles * sizeof(ino_t));
    genlist = (__u32 *)malloc(nfiles * sizeof(__u32));
    ret = (xfs_bstat_t *)malloc(nfiles * sizeof(xfs_bstat_t));

    for (k=0; k < iterations; k++) {
	xfs_ino_t last_inode = 0;
	int count = 0;
	int testFiles = 0;

	printf("Iteration %d ... \n", k);

	memset(inodelist, 0, nfiles * sizeof(ino_t));
	memset(genlist, 0, nfiles * sizeof(__u32));
	memset(ret, 0, nfiles * sizeof(xfs_bstat_t));
	memset(&a, 0, sizeof(xfs_fsop_bulkreq_t));
	a.lastip = (__u64 *)&last_inode;
	a.icount = nfiles;
	a.ubuffer = ret;
	a.ocount = &count;

	if (mkdir(dirname, 0755) < 0) {
	    perror(dirname);
	    exit(1);
	}

	/* create nfiles and store their inode numbers in inodelist */
	for (i=0; i < nfiles; i++) {
	    sprintf(fname, "%s/file%06d", dirname, i);
	    if ((fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		perror(fname);
		exit(1);
	    }
	    write(fd, fname, sizeof(fname));
	    if (fstat(fd, &sbuf) < 0) {
		perror(fname);
		exit(1);
	    }
	    inodelist[i] = sbuf.st_ino;
	    close(fd);
	}
	
	sync();
	
	/* collect bs_gen for the nfiles files */
	if ((fd = open(dirname, O_RDONLY)) < 0) {
	    perror(dirname);
	    exit(1);
	}

	testFiles = 0;
	for (;;) {
	    if ((e = xfsctl(dirname, fd, XFS_IOC_FSBULKSTAT, &a)) < 0) {
		perror("XFS_IOC_FSBULKSTAT1:");
		exit(1);
	    }

	    if (count == 0)
		break;

	    for (i=0; i < count; i++) {
		for (j=0; j < nfiles; j += stride) {
		    if (ret[i].bs_ino == inodelist[j]) {
			genlist[j] = ret[i].bs_gen;
			testFiles++;
		    }
		}
	    }
	}
	close(fd);
	
	printf("testFiles %d ... \n", testFiles);

	/* remove some of the first set of files */
	for (i=0; i < nfiles; i += stride) {
	    sprintf(fname, "%s/file%06d", dirname, i);
	    if (unlink(fname) < 0) {
	    	perror(fname);
		exit(1);
	    }
	}

	/* create a new set of files (replacing the unlinked ones) */
	for (i=0; i < nfiles; i += stride) {
	    sprintf(fname, "%s/file%06d", dirname, i);
	    if ((fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		perror(fname);
		exit(1);
	    }
	    write(fd, fname, sizeof(fname));
	    close(fd);
	}

	sync();
	last_inode = 0; count = 0;

	if ((fd = open(dirname, O_RDONLY)) < 0) {
	    perror(dirname);
	    exit(1);
	}

	for (;;) {
	    if ((e = xfsctl(dirname, fd, XFS_IOC_FSBULKSTAT, &a)) < 0) {
		perror("XFS_IOC_FSBULKSTAT:");
		exit(1);
	    }

	    if (count == 0)
		    break;

	    for (i=0; i < count; i++) {
		for (j=0; j < nfiles; j += stride) {
		    if ((ret[i].bs_ino == inodelist[j]) &&
			(ret[i].bs_gen == genlist[j])) {
			/* oops, the same inode with old gen number */
			printf("Unlinked inode %llu with generation %d "
			       "returned by bulkstat\n",
				(unsigned long long)inodelist[j],
				 genlist[j]);
			exit(1);
		    }
		    if ((ret[i].bs_ino == inodelist[j])) {
			if ((genlist[j] + 1) != ret[i].bs_gen) {
				/* oops, the new gen number is not 1 bigger than the old */
				printf("Inode with old generation %d, new generation %d\n",
				genlist[j], ret[i].bs_gen);
				exit(1);
			}
		    }
		}
	    }
	}

	close(fd);

	sprintf(fname, "rm -rf %s\n", dirname);
	system(fname);

	sync();
	sleep(2);
	printf("passed\n");
    }

    exit(0);
}
