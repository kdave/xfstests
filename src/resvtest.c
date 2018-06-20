// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 */ 

#include "global.h"

int
main(int argc, char **argv)
{
	int		c, i, j, n, err = 0;
	int		writefd, readfd;
	long		iterations = 100;
	long		psize, bsize, leaksize = 32 * 1024 * 1024;
	char		*filename;
	char		*readbuffer, *writebuffer;
	off64_t		resvsize;
	xfs_flock64_t	resvsp;

	psize = bsize = getpagesize();
	resvsize = bsize * (off64_t) 10000;

	while ((c = getopt(argc, argv, "b:i:l:s:")) != EOF) {
		switch(c) {
		case 'b':
			bsize = atol(optarg);
			break;
		case 'i':
			iterations = atol(optarg);
			break;
		case 'l':
			leaksize = atol(optarg);
			break;
		case 's':
			resvsize = (off64_t) atoll(optarg);
			break;
		default:
			err++;
			break;
		}
	}

	if (optind > argc + 1)
		err++;

	if (err) {
		printf("Usage: %s [-b blksize] [-l leaksize] [-r resvsize]\n",
			argv[0]);
		exit(0);
	}

	filename = argv[optind];

	readbuffer = memalign(psize, bsize);
	writebuffer = memalign(psize, bsize);
	if (!readbuffer || !writebuffer) {
		perror("open");
		exit(1);
	}
	memset(writebuffer, 'A', bsize);

	unlink(filename);
	writefd = open(filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (writefd < 0) {
		perror("open");
		exit(1);
	}
	readfd = open(filename, O_RDONLY);
	if (readfd < 0) {
		perror("open");
		exit(1);
	}

	/* preallocate file space */
	resvsp.l_whence = 0;
	resvsp.l_start = 0;
	resvsp.l_len = resvsize;
	if (xfsctl(filename, writefd, XFS_IOC_RESVSP64, &resvsp) < 0) {
		fprintf(stdout, "attempt to reserve %lld bytes for %s "
		                "using %s failed: %s (%d)\n", 
				(long long int)resvsize, filename, "XFS_IOC_RESVSP64",
				strerror(errno), errno);
	} else {
		fprintf(stdout, "reserved %lld bytes for %s using %s\n",
				(long long int)resvsize, filename, "XFS_IOC_RESVSP64");
	}

	/* Space is now preallocated, start IO --
	 * write at current offset, pressurize, seek to zero on reader
	 * and read up to current write offset.
	 */

	n = 0;
	while (++n < iterations) {
		char *p;
		int numerrors;

		if (write(writefd, writebuffer, bsize) < 0) {
			perror("write");
			exit(1);
		}

		/* Apply some memory pressure 
		 * (allocate another chunk and touch all pages)
		 */
		for (i = 0; i < (leaksize / psize); i++) {
			p = malloc(psize);
			if (p)
				p[7] = '7';
		}
		sleep(1);
		lseek(readfd, SEEK_SET, 0);
		numerrors = 0;
		for (j = 0; j < n; j++) {
			if (read(readfd, readbuffer, bsize) < 0) {
				perror("read");
				exit(1);
			}
			for (i = 0; i < bsize; i++) {
				if (readbuffer[i] != 'A') {
					fprintf(stderr,
"errors detected in file, pos: %d (%lld+%d), nwrites: %d [val=0x%x].\n",
						j, (long long)j * 4096,
						i, n, readbuffer[i]);
					numerrors++;
					break;
				}
			}
		}
		if (numerrors > 10) {
			exit(1);
		} else if (numerrors) {
			fprintf(stdout, "\n");
		}
	}

	return(0);
}
