/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
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
 
#include "global.h"

int verbose;		/* print lots of debugging info */

void usage(char *progname);
void writeblks(int fd, long filesize, int blocksize,
		   int interleave, int base, int rev);
int readblks(int fd, long filesize, int blocksize,
		  int interleave, int count);

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-l filesize] [-b blocksize] [-i interleave]\n"
			"\t\t[-c count] [-r(everse)] [-v(erbose)] filename\n",
			progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int interleave, blocksize, count, rev, i, ch, fd;
	long filesize;
	char *filename = NULL;
        int errs;

	filesize = 1024*1024;
	blocksize = 4096;
	count = interleave = 4;
	rev = verbose = 0;
	while ((ch = getopt(argc, argv, "b:l:i:c:rv")) != EOF) {
		switch(ch) {
		case 'b':	blocksize  = atoi(optarg);	break;
		case 'c':	count      = atoi(optarg);	break;
		case 'i':	interleave = atoi(optarg);	break;
		case 'l':	filesize   = atol(optarg);	break;
		case 'v':	verbose++;			break;
		case 'r':	rev++;				break;
		default:	usage(argv[0]);			break;
		}
	}
	if (optind == argc-1)
		filename = argv[optind];
	else
		usage(argv[0]);
	if ((filesize % (blocksize*interleave)) != 0) {
		filesize -= filesize % (blocksize * interleave);
		printf("filesize not a multiple of blocksize*interleave,\n");
		printf("reducing filesize to %ld\n", filesize);
	}
	if (count > interleave) {
		count = interleave;
		printf("count of passes is too large, setting to %d\n", count);
	} else if (count < 1) {
		count = 1;
		printf("count of passes is too small, setting to %d\n", count);
	}

	if ((fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
		perror("open");
		return 1;
	}

	/*
	 * Avoid allocation patterns being perturbed by different speculative
	 * preallocation beyond EOF configurations by first truncating the file
	 * to the expected maximum file size.
	 */
	if (ftruncate(fd, filesize) < 0) {
		perror("ftruncate");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < count; i++) {
		writeblks(fd, filesize, blocksize, interleave, i, rev);
	}
	errs=readblks(fd, filesize, blocksize, interleave, count);
	if (close(fd) < 0) {
		perror("close");
		return 1;
	}
        if (errs) {
            printf("%d errors detected during readback\n", errs);
            return 1;
        }
	return 0;
}

void
writeblks(int fd, long filesize, int blocksize, int interleave, int base, int rev)
{
	long offset;
	char *buffer;

	if ((buffer = calloc(1, blocksize)) == NULL) {
		perror("malloc");
		exit(1);
	}

	if (rev) {
		offset = (filesize-blocksize) - (base * blocksize);
	} else {
		offset = base * blocksize;
	}
	for (;;) {
		if (lseek(fd, offset, SEEK_SET) < 0) {
			perror("lseek");
			exit(1);
		}
		*(long *)buffer = *(long *)(buffer+256) = offset;
		if (write(fd, buffer, blocksize) < blocksize) {
			perror("write");
			exit(1);
		}
		if (verbose) {
			printf("writing data at offset=%ld, delta=%d, value 0x%lx and 0x%lx\n",
					offset-base*blocksize, base,
					*(long *)buffer,
					*(long *)(buffer+256));
		}

		if (rev) {
			offset -= interleave*blocksize;
			if (offset < 0)
				break;
		} else {
			offset += interleave*blocksize;
			if (offset >= filesize)
				break;
		}
	}
        
        {
            /* pad file out to full length */
            char *zero="";
            if (lseek(fd, filesize-1, SEEK_SET)<0) {
                perror("lseek");
                exit(1);
            }
            if (write(fd, zero, 1)!=1) {
                perror("write");
                exit(1);
            }
        }

	free(buffer);
}

int
readblks(int fd, long filesize, int blocksize, int interleave, int count)
{
	long offset;
	char *buffer, *tmp;
	int xfer, i;
        int errs=0;

	if ((buffer = calloc(interleave, blocksize)) == NULL) {
		perror("malloc");
		exit(1);
	}
	xfer = interleave * blocksize;

	if (lseek(fd, (long)0, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}
	for (offset = 0; offset < filesize; offset += xfer) {
		if ((i = read(fd, buffer, xfer) < xfer)) {
			if (i == 0)
				break;
                        if (i < 0)
   			        perror("read");
                        printf("short read: %d of %d bytes read\n", i, xfer);
                        
			exit(1);
		}
		for (tmp = buffer, i = 0; i < count; i++, tmp += blocksize) {
			if ( (*(long *)tmp != (offset+i*blocksize)) ||
			     (*(long *)(tmp+256) != (offset+i*blocksize)) ) {
				printf("mismatched data at offset=%ld, delta=%d, expected 0x%lx, got 0x%lx and 0x%lx\n",
						   offset, i,
						   offset+i*blocksize,
						   *(long *)tmp,
						   *(long *)(tmp+256));
                                errs++;
			}
		}
	}

	free(buffer);
        return errs;
}
