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
 
#include "global.h"

unsigned char	*valid;	/* Bit-vector array showing which blocks have been written */
int		nvalid;	/* number of bytes in valid array */
#define	SETBIT(ARRAY, N)	((ARRAY)[(N)/8] |= (1 << ((N)%8)))
#define	BITVAL(ARRAY, N)	((ARRAY)[(N)/8] & (1 << ((N)%8)))

off64_t filesize;
int blocksize;
int count;
int verbose;
int wsync;
int direct;
int alloconly;
int rt;
int extsize;
int preserve;
int test;
off64_t fileoffset;
struct dioattr diob;
struct fsxattr rtattr;

#define	READ_XFER	10	/* block to read at a time when checking */

void usage(char *progname);
int findblock(void);
void writeblks(int fd);
int readblks(int fd);
void dumpblock(int *buffer, off64_t offset, int blocksize);

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-l filesize] [-b blocksize] [-c count] [-o write offset] [-s seed] [-x extentsize] [-w] [-v] [-d] [-r] [-a] [-p] filename\n",
			progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int seed, ch, fd, oflags;
	char *filename;
        int r;

	filesize = ((off64_t)256)*1024*1024;
	blocksize = 512;
	count = filesize/blocksize;
	verbose = 0;
	wsync = 0;
	seed = time(NULL);
        test = 0;
	while ((ch = getopt(argc, argv, "b:l:s:c:o:x:vwdrapt")) != EOF) {
		switch(ch) {
		case 'b':	blocksize  = atoi(optarg);	break;
		case 'l':	filesize   = strtoll(optarg, NULL, 16); break;
		case 's':	seed       = atoi(optarg);	break;
		case 'c':	count      = atoi(optarg);	break;
		case 'o':	fileoffset = strtoll(optarg, NULL, 16); break;
		case 'x':	extsize    = atoi(optarg);	break;
		case 'v':	verbose++;			break;
		case 'w':	wsync++;			break;
		case 'd':	direct++;			break;
		case 'r':	rt++;				break;
		case 'a':	alloconly++;			break;
		case 'p':	preserve++;			break;
                case 't':       test++; preserve++;             break;
		default:	usage(argv[0]);			break;
		}
	}
	if (optind == argc-1)
		filename = argv[optind];
	else
		usage(argv[0]);
	if ((filesize % blocksize) != 0) {
		filesize -= filesize % blocksize;
		printf("filesize not a multiple of blocksize, reducing filesize to %lld\n",
				 filesize);
	}
	if ((fileoffset % blocksize) != 0) {
		fileoffset -= fileoffset % blocksize;
		printf("fileoffset not a multiple of blocksize, reducing fileoffset to %lld\n",
		       fileoffset);
	}
	if (count > (filesize/blocksize)) {
		count = (filesize/blocksize);
		printf("count of blocks written is too large, setting to %d\n",
			      count);
	} else if (count < 1) {
		count = 1;
		printf("count of blocks written is too small, setting to %d\n",
			      count);
	}
	printf("randholes: Seed = %d (use \"-s %d\" to re-execute this test)\n", seed, seed);
	srandom(seed);
        
        printf("randholes: blocksize=%d, filesize=%Ld, seed=%d\n"
               "randholes: count=%d, offset=%Ld, extsize=%d\n",
                blocksize, filesize, seed, count, fileoffset, extsize);
        printf("randholes: verbose=%d, wsync=%d, direct=%d, rt=%d, alloconly=%d, preserve=%d, test=%d\n",
                verbose, wsync, direct, rt, alloconly, preserve, test);
        
	/*
	 * Open the file, write rand block in random places, read them all
	 * back to check for correctness, then close the file.
	 */
	nvalid = (filesize / blocksize) / 8 + 1;
	if ((valid = (unsigned char *)calloc(1, (unsigned)nvalid)) == NULL) {
		perror("malloc");
		return 1;
	}
	if (rt)
		direct++;
        
        oflags=test?(O_RDONLY):(O_RDWR | O_CREAT);
	oflags |=   (preserve ? 0 : O_TRUNC) |
		    (wsync ? O_SYNC : 0) |
		    (direct ? O_DIRECT : 0);
        
	if ((fd = open(filename, oflags, 0666)) < 0) {
		perror("open");
		return 1;
	}
	if (rt) {
		if (ioctl(fd, XFS_IOC_FSGETXATTR, &rtattr) < 0) {
			perror("ioctl(XFS_IOC_FSGETXATTR)");
			return 1;
		}
		if ((rtattr.fsx_xflags & XFS_XFLAG_REALTIME) == 0 ||
		    (extsize && rtattr.fsx_extsize != extsize * blocksize)) {
			rtattr.fsx_xflags |= XFS_XFLAG_REALTIME;
			if (extsize)
				rtattr.fsx_extsize = extsize * blocksize;
			if (ioctl(fd, XFS_IOC_FSSETXATTR, &rtattr) < 0) {
				perror("ioctl(XFS_IOC_FSSETXATTR)");
				return 1;
			}
		}
	}
	if (direct) {
		if (ioctl(fd, XFS_IOC_DIOINFO, &diob) < 0) {
			perror("ioctl(XFS_IOC_FIOINFO)");
			return 1;
		}
		if (blocksize % diob.d_miniosz) {
			fprintf(stderr, "blocksize %d must be a multiple of %d for direct I/O\n", blocksize, diob.d_miniosz);
			return 1;
		}
	}
        printf(test?"write (skipped)\n":"write\n");
	writeblks(fd);
        printf("readback\n");
	r=readblks(fd);
	if (close(fd) < 0) {
		perror("close");
		return 1;
	}
	free(valid);
        
        if (r) {
            printf("randholes: %d errors found during readback\n", r);
            return 2;
        } else {
            printf("randholes: ok\n");
            return 0;
        }
}

void
writeblks(int fd)
{
	off64_t offset;
	char *buffer;
	int block;
	struct flock64 fl;

	if (direct)
		buffer = memalign(diob.d_mem, blocksize);
	else
		buffer = malloc(blocksize);
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}
	memset(buffer, 0, blocksize);

	for (  ; count > 0; count--) {
		if (verbose && ((count % 100) == 0)) {
			printf(".");
			fflush(stdout);
		}
		block = findblock();
		offset = (off64_t)block * blocksize;
		if (alloconly) {
                        if (test) continue;
                        
			fl.l_start = offset;
			fl.l_len = blocksize;
			fl.l_whence = 0;
			if (ioctl(fd, XFS_IOC_RESVSP64, &fl) < 0) {
				perror("ioctl(XFS_IOC_RESVSP64)");
				exit(1);
			}
			continue;
		}
		SETBIT(valid, block);
                if (!test) {
		        if (lseek64(fd, fileoffset + offset, SEEK_SET) < 0) {
			        perror("lseek");
			        exit(1);
		        }
                }
		*(off64_t *)buffer = *(off64_t *)(buffer+256) =
			fileoffset + offset;
                if (!test) {
		        if (write(fd, buffer, blocksize) < blocksize) {
			        perror("write");
			        exit(1);
		        }
                }
                if (test && verbose>1) printf("NOT ");
		if (verbose > 1) {
			printf("writing data at offset=%llx, value 0x%llx and 0x%llx\n",
			       fileoffset + offset,
			       *(off64_t *)buffer, *(off64_t *)(buffer+256));
		}
	}

	free(buffer);
}

int
readblks(int fd)
{
	long offset;
	char *buffer, *tmp;
	int xfer, block, i;
        int err=0;

	if (alloconly)
		return 0;
	xfer = READ_XFER*blocksize;
	if (direct)
		buffer = memalign(diob.d_mem, xfer);
	else
		buffer = malloc(xfer);
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}
	memset(buffer, 0, xfer);
	if (verbose)
		printf("\n");

	if (lseek64(fd, fileoffset, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}
	for (offset = 0, block = 0; offset < filesize; offset += xfer) {
		if ((i = read(fd, buffer, xfer) < xfer)) {
			if (i < 2)
				break;
			perror("read");
			exit(1);
		}
		for (tmp = buffer, i = 0; i < READ_XFER; i++, block++, tmp += blocksize) {
			if (verbose && ((block % 100) == 0)) {
				printf("+");
				fflush(stdout);
			}
			if (BITVAL(valid, block) == 0) {
				if ((*(off64_t *)tmp != 0LL) ||
				    (*(off64_t *)(tmp+256) != 0LL)) {
					printf("mismatched data at offset=%llx, expected 0x%llx, got 0x%llx and 0x%llx\n",
					       fileoffset + block * blocksize,
					       0LL,
					       *(off64_t *)tmp,
					       *(off64_t *)(tmp+256));
                                        err++;
				}
			} else {
				if ( (*(off64_t *)tmp !=
				      fileoffset + block * blocksize) ||
				     (*(off64_t *)(tmp+256) !=
				      fileoffset + block * blocksize) ) {
					printf("mismatched data at offset=%llx, expected 0x%llx, got 0x%llx and 0x%llx\n",
					       fileoffset + block * blocksize,
					       fileoffset + block * blocksize,
					       *(off64_t *)tmp,
					       *(off64_t *)(tmp+256));
                                        err++;
				}
			}
			if (verbose > 2) {
				printf("block %d blocksize %d\n", block,
				       blocksize);
				dumpblock((int *)tmp,
					  fileoffset + block * blocksize,
					  blocksize);
			}
		}
	}
	if (verbose)
		printf("\n");

	free(buffer);
        return err;
}

int
findblock(void)
{
	int block, numblocks;

	numblocks = filesize / blocksize;
	block = random() % numblocks;
	if (BITVAL(valid, block) == 0)
		return(block);

	for (  ; BITVAL(valid, block) != 0; block++) {
		if (block == (numblocks-1))
			block = -1;
	}
	if (block == -1)
		printf("returning block -1\n");
	return(block);
}

void
dumpblock(int *buffer, off64_t offset, int blocksize)
{
	int	i;

	for (i = 0; i < (blocksize / 16); i++) {
		printf("%llx: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		       offset, *buffer, *(buffer + 1), *(buffer + 2),
		       *(buffer + 3));
		offset += 16;
		buffer += 4;
	}
}
