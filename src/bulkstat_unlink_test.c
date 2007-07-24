/*
 * $Id: bulkstat_unlink_test.c,v 1.1 2007/07/24 16:08:02 mohamedb.longdrop.melbourne.sgi.com Exp $
 * Test bulkstat doesn't returned unlinked inodes.
 * Mark Goodwin <markgw@sgi.com> Fri Jul 20 09:13:57 EST 2007
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xfs/xfs.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    int e;
    int fd;
    int i;
    int j;
    int k;
    int nfiles;  
    int stride;
    struct stat sbuf;
    ino_t *inodelist;
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

    inodelist = (ino_t *)malloc(nfiles * sizeof(ino_t));
    ret = (xfs_bstat_t *)malloc(nfiles * sizeof(xfs_bstat_t));

    for (k=0; k < iterations; k++) {
	xfs_ino_t last_inode = 0;
	int count = 0;

	printf("Iteration %d ... ", k);

	memset(&a, 0, sizeof(xfs_fsop_bulkreq_t));
	a.lastip = &last_inode;
	a.icount = nfiles;
	a.ubuffer = ret;
	a.ocount = &count;

	if (mkdir(dirname, 0755) < 0) {
	    perror(dirname);
	    exit(1);
	}

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

	if ((fd = open(dirname, O_RDONLY)) < 0) {
	    perror(dirname);
	    exit(1);
	}

	/*
	 * test begins here
	 */
	for (i=0; i < nfiles; i += stride) {
	    sprintf(fname, "%s/file%06d", dirname, i);
	    if (unlink(fname) < 0) {
	    	perror(fname);
		exit(1);
	    }
	}

	sync();

	for (;;) {
	    if ((e = xfsctl(dirname, fd, XFS_IOC_FSBULKSTAT, &a)) < 0) {
		perror("XFS_IOC_FSBULKSTAT:");
		exit(1);
	    }

	    if (count == 0)
		break;

	    for (i=0; i < count; i++) {
		for (j=0; j < nfiles; j += stride) {
		    if (ret[i].bs_ino == inodelist[j]) {
			/* oops ... */
			printf("failed. Unlinked inode %ld returned by bulkstat\n", inodelist[j]); 
			exit(1);
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
