/*
 * Copyright (c) 2000-2003, 2010 SGI
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
 
#include <inttypes.h>

#include "global.h"

#define	power_of_2(x)	((x) && !((x) & ((x) - 1)))
#define	DEFAULT_FILESIZE	((__uint64_t) (256 * 1024 * 1024))
#define	DEFAULT_BLOCKSIZE	512

#define	SETBIT(ARRAY, N)	((ARRAY)[(N)/8] |= (1 << ((N)%8)))
#define	BITVAL(ARRAY, N)	((ARRAY)[(N)/8] & (1 << ((N)%8)))

/* Bit-vector array showing which blocks have been written */
static unsigned char	*valid;

static __uint64_t filesize;
static __uint64_t fileoffset;

static unsigned int blocksize;
static int count;
static int verbose;
static int wsync;
static int direct;
static int alloconly;
static int rt;
static int extsize;	/* used only for real-time */
static int preserve;
static int test;

#define	READ_XFER	256	/* blocks to read at a time when checking */

/*
 * Define xfscntl() to mask the difference between the Linux
 * and the Irix fcntl() interfaces to XFS for user space.  The
 * "cmd" argument is just the last part of the command, e.g.
 * pass FSGETXATTR in place of either XFS_IOC_FSGETXATTR (Linux)
 * F_FSGETXATTR (Irix).
 *
 */
#  define xfscntl(filename, fd, cmd, arg) \
		xfsctl((filename), (fd), XFS_IOC_ ## cmd, (arg))

static void
usage(char *progname)
{
	fprintf(stderr,
		"usage: %s [-l filesize] [-b blocksize] [-c count]\n"
		"\t\t[-o write_offset] [-s seed] [-r [-x extentsize]]\n"
		"\t\t[-w] [-v] [-d] [-a] [-p] [-t] filename\n\n",
		progname);
	fprintf(stderr, "\tdefault filesize is %" PRIu64 " bytes\n",
		DEFAULT_FILESIZE);
	fprintf(stderr, "\tdefault blocksize is %u bytes\n",
		DEFAULT_BLOCKSIZE);
	fprintf(stderr, "\tdefault count is %d block-sized writes\n",
		(int) (DEFAULT_FILESIZE / DEFAULT_BLOCKSIZE));
	fprintf(stderr, "\tdefault write_offset is %" PRIu64 " bytes\n",
		(__uint64_t) 0);
	exit(1);
}

/* Returns filename if successful or a null pointer if an error occurs */
static char *
parseargs(int argc, char *argv[])
{
	int seed;
	int ch;

	filesize = DEFAULT_FILESIZE;
	blocksize = DEFAULT_BLOCKSIZE;
	count = (int) filesize / blocksize;
	verbose = 0;
	wsync = 0;
	seed = time(NULL);
	test = 0;
	while ((ch = getopt(argc, argv, "b:l:s:c:o:x:vwdrapt")) != EOF) {
		switch(ch) {
		case 'b':	blocksize  = atoi(optarg);	break;
		case 'l':	filesize   = strtoull(optarg, NULL, 16); break;
		case 's':	seed       = atoi(optarg);	break;
		case 'c':	count      = atoi(optarg);	break;
		case 'o':	fileoffset = strtoull(optarg, NULL, 16); break;
		case 'x':	extsize    = atoi(optarg);	break;
		case 'v':	verbose++;			break;
		case 'w':	wsync++;			break;
		case 'd':	direct++;			break;
		case 'r':	rt++; direct++;			break;
		case 'a':	alloconly++;			break;
		case 'p':	preserve++;			break;
		case 't':	test++; preserve++;		break;
		default:	usage(argv[0]);			break;
		}
	}
	if (optind != argc - 1)
		usage(argv[0]);

	if ((filesize % blocksize) != 0) {
		filesize -= filesize % blocksize;
		printf("filesize not a multiple of blocksize, "
			"reducing filesize to %llu\n",
		       (unsigned long long) filesize);
	}
	if ((fileoffset % blocksize) != 0) {
		fileoffset -= fileoffset % blocksize;
		printf("fileoffset not a multiple of blocksize, "
			"reducing fileoffset to %llu\n",
		       (unsigned long long) fileoffset);
	}
	if (count > (filesize/blocksize)) {
		count = (filesize/blocksize);
		printf("count of blocks written is too large, "
			"setting to %d\n", count);
	} else if (count < 1) {
		count = 1;
		printf("count of blocks written is too small, "
			"setting to %d\n", count);
	}
	printf("randholes: Seed = %d (use \"-s %d\" "
		"to re-execute this test)\n", seed, seed);
	srandom(seed);

        printf("randholes: blocksize=%d, filesize=%llu, seed=%d\n"
               "randholes: count=%d, offset=%llu, extsize=%d\n",
                blocksize, (unsigned long long)filesize, seed,
	       count, (unsigned long long)fileoffset, extsize);
        printf("randholes: verbose=%d, wsync=%d, direct=%d, "
		"rt=%d, alloconly=%d, preserve=%d, test=%d\n",
                verbose, wsync, direct ? 1 : 0, rt, alloconly, preserve, test);

	/* Last argument is the file name.  Return it. */

	return argv[optind];	/* Success */
}

/*
 * Determine the next random block number to which to write.
 * If an already-written block is selected, choose the next
 * unused higher-numbered block.  Returns the block number,
 * or -1 if we exhaust available blocks looking for an unused
 * one.
 */
static int
findblock(void)
{
	int block, numblocks;

	numblocks = filesize / blocksize;
	block = random() % numblocks;

	while (BITVAL(valid, block)) {
		if (++block == numblocks) {
			printf("returning block -1\n");
			return -1;
		}
	}
	return block;
}

static void
dumpblock(int *buffer, __uint64_t offset, int blocksize)
{
	int	i;

	for (i = 0; i < (blocksize / 16); i++) {
		printf("%llx: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		       (unsigned long long) offset, *buffer, *(buffer + 1),
		       *(buffer + 2), *(buffer + 3));
		offset += 16;
		buffer += 4;
	}
}

static void
writeblks(char *fname, int fd, size_t alignment)
{
	__uint64_t offset;
	char *buffer = NULL;
	int block;
	int ret;
	struct flock64 fl;

	if (!test) {
		ret = posix_memalign((void **) &buffer, alignment, blocksize);
		if (ret) {
			fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
			exit(1);
		}
		memset(buffer, 0, blocksize);
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

	do {
		if (verbose && ((count % 100) == 0)) {
			printf(".");
			fflush(stdout);
		}
		block = findblock();
		if (block < 0) {
		    perror("findblock");
		    exit(1);
		}

		offset = (__uint64_t) block * blocksize;
		if (alloconly) {
                        if (test) continue;
                        
			fl.l_start = fileoffset + offset;
			fl.l_len = blocksize;
			fl.l_whence = 0;

			if (xfscntl(fname, fd, RESVSP64, &fl) < 0) {
				perror("xfsnctl(RESVSP64)");
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
			/*
			 * Before writing, record offset at the base
			 * of the buffer and at offset 256 bytes
			 * into it.  We'll verify this when we read
			 * it back in again.
			 */
			*(__uint64_t *) buffer = fileoffset + offset;
			*(__uint64_t *) (buffer + 256) = fileoffset + offset;

		        if (write(fd, buffer, blocksize) < blocksize) {
			        perror("write");
			        exit(1);
		        }
                }
		if (verbose > 1) {
			printf("%swriting data at offset=%llx\n",
				test ? "NOT " : "",
				(unsigned long long) (fileoffset + offset));
		}
	} while (--count);

	free(buffer);
}

static int
readblks(int fd, size_t alignment)
{
	__uint64_t offset;
	char *buffer, *tmp;
	unsigned int xfer, block, i;
        int err=0;

	if (alloconly)
		return 0;
	xfer = READ_XFER*blocksize;
	err = posix_memalign((void **) &buffer, alignment, xfer);
	if (err) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(err));
		exit(1);
	}
	memset(buffer, 0, xfer);
	if (verbose)
		printf("\n");

	if (lseek64(fd, fileoffset, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}
	block = 0;
	offset = 0;
	while (offset < filesize) {
		if ((i = read(fd, buffer, xfer) < xfer)) {
			if (i < 2)
				break;
			perror("read");
			exit(1);
		}
		tmp = buffer;
		for (i = 0; i < READ_XFER; i++) {
			__uint64_t want;
			__uint64_t first;
			__uint64_t second;

			if (verbose && ((block % 100) == 0)) {
				printf("+");
				fflush(stdout);
			}

			want = BITVAL(valid, block) ? offset : 0;
			first = *(__uint64_t *) tmp;
			second = *(__uint64_t *) (tmp + 256);
			if (first != want || second != want) {
				printf("mismatched data at offset=0x%" PRIx64
					", expected 0x%" PRIx64
					", got 0x%" PRIx64
					" and 0x%" PRIx64 "\n",
					 fileoffset + offset, want,
					 first, second);
				err++;
			}
			if (verbose > 2) {
				printf("block %d blocksize %d\n", block,
				       blocksize);
				dumpblock((int *)tmp, fileoffset + offset,
					  blocksize);
			}

			block++;
			offset += blocksize;
			tmp += blocksize;
		}
	}
	if (verbose)
		printf("\n");

	free(buffer);
        return err;
}

/*
 * Determine the memory alignment required for I/O buffers.  For
 * direct I/O we request the needed information from the file
 * system; otherwise pointer alignment is fine.  Returns the
 * alignment multiple, or 0 if an error occurs.
 */
static size_t
get_alignment(char *filename, int fd)
{
	struct dioattr dioattr;

	if (! direct)
		return sizeof (void *);

	memset(&dioattr, 0, sizeof dioattr);
	if (xfscntl(filename, fd, DIOINFO, &dioattr) < 0) {
		perror("xfscntl(FIOINFO)");
		return 0;
	}

	/* Make sure the alignment meets the needs of posix_memalign() */

	if (dioattr.d_mem % sizeof (void *) || ! power_of_2(dioattr.d_mem)) {
		perror("d_mem bad");
		return 0;
	}

	/*
	 * Also make sure user doesn't specify a block size that's
	 * incompatible with the underlying file system.
	 */
	if (! dioattr.d_miniosz) {
		perror("miniosz == 0!");
		return 0;
	}
	if (blocksize % dioattr.d_miniosz) {
		fprintf(stderr, "blocksize %d must be a multiple of "
			"%d for direct I/O\n", blocksize, dioattr.d_miniosz);
		return 0;
	}

	return (size_t) dioattr.d_mem;
}

static int
realtime_setup(char *filename, int fd)
{
	struct fsxattr rtattr;

	(void) memset(&rtattr, 0, sizeof rtattr);
	if (xfscntl(filename, fd, FSGETXATTR, &rtattr) < 0) {
		perror("FSGETXATTR)");
		return 1;
	}
	if ((rtattr.fsx_xflags & XFS_XFLAG_REALTIME) == 0 ||
	    (extsize && rtattr.fsx_extsize != extsize * blocksize)) {
		rtattr.fsx_xflags |= XFS_XFLAG_REALTIME;
		if (extsize)
			rtattr.fsx_extsize = extsize * blocksize;
		if (xfscntl(filename, fd, FSSETXATTR, &rtattr) < 0) {
			perror("FSSETXATTR)");
			return 1;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	char	*filename;
	size_t	size;
	int	oflags;
	int	fd;
	size_t	alignment;
	int	errors;

	filename = parseargs(argc, argv);
	if (! filename)
		return 1;

	/*
	 * Allocate a bitmap big enough to track the range of
	 * blocks we'll be dealing with.
	 */
	size = (filesize / blocksize) / 8 + 1;
	valid = malloc(size);
	if ((valid = malloc(size)) == NULL) {
		perror("malloc");
		return 1;
	}
	memset(valid, 0, size);

	/* Lots of arguments affect how we open the file */
	oflags = test ? O_RDONLY : O_RDWR|O_CREAT;
	oflags |= preserve ? 0 : O_TRUNC;
	oflags |= wsync ? O_SYNC : 0;
	oflags |= direct ? O_DIRECT : 0;

	/*
	 * Open the file, write rand block in random places, read them all
	 * back to check for correctness, then close the file.
	 */
	if ((fd = open(filename, oflags, 0666)) < 0) {
		perror("open");
		return 1;
	}
	if (rt && realtime_setup(filename, fd))
		return 1;
	alignment = get_alignment(filename, fd);
	if (! alignment)
		return 1;

        printf("write%s\n", test ? " (skipped)" : "");
	writeblks(filename, fd, alignment);

        printf("readback\n");
	errors = readblks(fd, alignment);

	if (close(fd) < 0) {
		perror("close");
		return 1;
	}
	free(valid);

        if (errors) {
		printf("randholes: %d errors found during readback\n", errors);
		return 2;
        }

	printf("randholes: ok\n");

	return 0;
}

