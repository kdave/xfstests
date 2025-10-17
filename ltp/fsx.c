/*
 *	Copyright (C) 1991, NeXT Computer, Inc.  All Rights Reserverd.
 *
 *	File:	fsx.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	File system exerciser. 
 *
 *	Rewritten 8/98 by Conrad Minshall.
 *
 *	Small changes to work under Linux -- davej.
 *
 *	Checks for mmap last-page zero fill.
 */

#include "global.h"

#include <limits.h>
#include <time.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/uio.h>
#include <stdbool.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#ifdef AIO
#include <libaio.h>
#endif
#ifdef URING
#include <liburing.h>
#endif
#include <sys/syscall.h>
#include "statx.h"

#ifndef MAP_FILE
# define MAP_FILE 0
#endif

#ifndef RWF_DONTCACHE
#define RWF_DONTCACHE	0x80
#endif

#ifndef RWF_ATOMIC
#define RWF_ATOMIC	0x40
#endif

#define NUMPRINTCOLUMNS 32	/* # columns of data to print on each line */

/* Operation flags (bitmask) */
enum opflags {
	FL_NONE = 0,
	FL_SKIPPED = 1,
	FL_CLOSE_OPEN = 2,
	FL_KEEP_SIZE = 4,
	FL_UNSHARE = 8
};

/*
 *	A log entry is an operation and a bunch of arguments.
 */

struct log_entry {
	int	operation;
	int	nr_args;
	int	args[4];
	enum opflags flags;
};

#define	LOGSIZE	10000

struct log_entry	oplog[LOGSIZE];	/* the log */
int			logptr = 0;	/* current position in log */
int			logcount = 0;	/* total ops */

/*
 * The operation matrix is complex due to conditional execution of different
 * features. Hence when we come to deciding what operation to run, we need to
 * be careful in how we select the different operations. The active operations
 * are mapped to numbers as follows:
 *
 *			lite	!lite	integrity
 * READ:		0	0	0
 * WRITE:		1	1	1
 * MAPREAD:		2	2	2
 * MAPWRITE:		3	3	3
 * TRUNCATE:		-	4	4
 * FALLOCATE:		-	5	5
 * PUNCH HOLE:		-	6	6
 * ZERO RANGE:		-	7	7
 * COLLAPSE RANGE:	-	8	8
 * FSYNC:		-	-	9
 *
 * When mapped read/writes are disabled, they are simply converted to normal
 * reads and writes. When fallocate/fpunch calls are disabled, they are
 * skipped.
 *
 * Because of the "lite" version, we also need to have different "maximum
 * operation" defines to allow the ops to be selected correctly based on the
 * mode being run.
 */

enum {
	/* common operations */
	OP_READ = 0,
	OP_READ_DONTCACHE,
	OP_WRITE,
	OP_WRITE_DONTCACHE,
	OP_WRITE_ATOMIC,
	OP_MAPREAD,
	OP_MAPWRITE,
	OP_MAX_LITE,

	/* !lite operations */
	OP_TRUNCATE = OP_MAX_LITE,
	OP_FALLOCATE,
	OP_PUNCH_HOLE,
	OP_ZERO_RANGE,
	OP_COLLAPSE_RANGE,
	OP_INSERT_RANGE,
	OP_CLONE_RANGE,
	OP_DEDUPE_RANGE,
	OP_COPY_RANGE,
	OP_EXCHANGE_RANGE,
	OP_MAX_FULL,

	/* integrity operations */
	OP_FSYNC = OP_MAX_FULL,
	OP_MAX_INTEGRITY,
};

#undef PAGE_SIZE
#define PAGE_SIZE       getpagesize()
#undef PAGE_MASK
#define PAGE_MASK       (PAGE_SIZE - 1)

char	*original_buf;			/* a pointer to the original data */
char	*good_buf;			/* a pointer to the correct data */
char	*temp_buf;			/* a pointer to the current data */
char	*fname;				/* name of our test file */
char	*bname;				/* basename of our test file */
char	*logdev;			/* -i flag */
char	*logid;				/* -j flag */
char	dname[1024];			/* -P flag */
char	goodfile[PATH_MAX];
int	dirpath = 0;			/* -P flag */
int	fd;				/* fd for our test file */

blksize_t	block_size = 0;
off_t		file_size = 0;
off_t		biggest = 0;
long long	testcalls = 0;		/* calls to function "test" */

long long	simulatedopcount = 0;	/* -b flag */
int	closeprob = 0;			/* -c flag */
int	debug = 0;			/* -d flag */
long long	debugstart = 0;		/* -D flag */
char	filldata = 0;			/* -g flag */
int	flush = 0;			/* -f flag */
int	do_fsync = 0;			/* -y flag */
unsigned long	maxfilelen = 256 * 1024;	/* -l flag */
int	sizechecks = 1;			/* -n flag disables them */
int	maxoplen = 64 * 1024;		/* -o flag */
int	quiet = 0;			/* -q flag */
long long	progressinterval = 0;	/* -p flag */
int	readbdy = 1;			/* -r flag */
int	style = 0;			/* -s flag */
int	prealloc = 0;			/* -x flag */
int	truncbdy = 1;			/* -t flag */
int	writebdy = 1;			/* -w flag */
long	monitorstart = -1;		/* -m flag */
long	monitorend = -1;		/* -m flag */
int	lite = 0;			/* -L flag */
long long numops = -1;			/* -N flag */
int	randomoplen = 1;		/* -O flag disables it */
int	seed = 1;			/* -S flag */
int     mapped_writes = 1;              /* -W flag disables */
int     fallocate_calls = 1;            /* -F flag disables */
int     keep_size_calls = 1;            /* -K flag disables */
int     unshare_range_calls = 1;        /* -u flag disables */
int     punch_hole_calls = 1;           /* -H flag disables */
int     zero_range_calls = 1;           /* -z flag disables */
int	collapse_range_calls = 1;	/* -C flag disables */
int	insert_range_calls = 1;		/* -I flag disables */
int	mapped_reads = 1;		/* -R flag disables it */
int	check_file = 0;			/* -X flag enables */
int	clone_range_calls = 1;		/* -J flag disables */
int	dedupe_range_calls = 1;		/* -B flag disables */
int	copy_range_calls = 1;		/* -E flag disables */
int	exchange_range_calls = 1;	/* -0 flag disables */
int	integrity = 0;			/* -i flag */
int	pollute_eof = 0;		/* -e flag */
int	fsxgoodfd = 0;
int	o_direct;			/* -Z */
int	aio = 0;
int	uring = 0;
int	mark_nr = 0;
int	dontcache_io = 1;
int	hugepages = 0;                  /* -h flag */
int	do_atomic_writes = 1;		/* -a flag disables */

/* User for atomic writes */
int awu_min = 0;
int awu_max = 0;

/* Stores info needed to periodically collapse hugepages */
struct hugepages_collapse_info {
	void *orig_good_buf;
	long good_buf_size;
	void *orig_temp_buf;
	long temp_buf_size;
};
struct hugepages_collapse_info hugepages_info;

int page_size;
int page_mask;
int mmap_mask;
int fsx_rw(int rw, int fd, char *buf, unsigned len, unsigned offset, int flags);
#define READ 0
#define WRITE 1
#define fsxread(a,b,c,d,f)	fsx_rw(READ, a,b,c,d,f)
#define fsxwrite(a,b,c,d,f)	fsx_rw(WRITE, a,b,c,d,f)

struct timespec deadline;

const char *replayops = NULL;
const char *recordops = NULL;
FILE *	fsxlogf = NULL;
FILE *	replayopsf = NULL;
char opsfile[PATH_MAX];
int badoff = -1;
int closeopen = 0;

static void *round_ptr_up(void *ptr, unsigned long align, unsigned long offset)
{
	unsigned long ret = (unsigned long)ptr;

	ret = roundup_64(ret, align);
	ret += offset;
	return (void *)ret;
}

void
vwarnc(int code, const char *fmt, va_list ap)
{
	if (logid)
		fprintf(stderr, "%s: ", logid);
	fprintf(stderr, "fsx: ");
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(code));
}

void
warn(const char * fmt, ...)  {
	va_list ap;
	va_start(ap, fmt);
	vwarnc(errno, fmt, ap);
	va_end(ap);
}

void
prt(const char *fmt, ...)
{
	va_list args;

	if (logid)
		fprintf(stdout, "%s: ", logid);
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	if (fsxlogf) {
		va_start(args, fmt);
		vfprintf(fsxlogf, fmt, args);
		va_end(args);
	}
}

void
prterr(const char *prefix)
{
	prt("%s%s%s\n", prefix, prefix ? ": " : "", strerror(errno));
}


static const char *op_names[] = {
	[OP_READ] = "read",
	[OP_READ_DONTCACHE] = "read_dontcache",
	[OP_WRITE] = "write",
	[OP_WRITE_DONTCACHE] = "write_dontcache",
	[OP_WRITE_ATOMIC] = "write_atomic",
	[OP_MAPREAD] = "mapread",
	[OP_MAPWRITE] = "mapwrite",
	[OP_TRUNCATE] = "truncate",
	[OP_FALLOCATE] = "fallocate",
	[OP_PUNCH_HOLE] = "punch_hole",
	[OP_ZERO_RANGE] = "zero_range",
	[OP_COLLAPSE_RANGE] = "collapse_range",
	[OP_INSERT_RANGE] = "insert_range",
	[OP_CLONE_RANGE] = "clone_range",
	[OP_DEDUPE_RANGE] = "dedupe_range",
	[OP_COPY_RANGE] = "copy_range",
	[OP_FSYNC] = "fsync",
	[OP_EXCHANGE_RANGE] = "exchange_range",
};

static const char *op_name(int operation)
{
	if (operation >= 0 &&
	    operation < sizeof(op_names) / sizeof(op_names[0]))
		return op_names[operation];
	return NULL;
}

static int op_code(const char *name)
{
	int i;

	for (i = 0; i < sizeof(op_names) / sizeof(op_names[0]); i++)
		if (op_names[i] && strcmp(name, op_names[i]) == 0)
			return i;
	return -1;
}

void
log5(int operation, int arg0, int arg1, int arg2, enum opflags flags)
{
	struct log_entry *le;

	le = &oplog[logptr];
	le->operation = operation;
	if (closeopen)
		flags |= FL_CLOSE_OPEN;
	le->args[0] = arg0;
	le->args[1] = arg1;
	le->args[2] = arg2;
	le->args[3] = file_size;
	le->nr_args = 4;
	le->flags = flags;
	logptr++;
	logcount++;
	if (logptr >= LOGSIZE)
		logptr = 0;
}

void
log4(int operation, int arg0, int arg1, enum opflags flags)
{
	struct log_entry *le;

	le = &oplog[logptr];
	le->operation = operation;
	if (closeopen)
		flags |= FL_CLOSE_OPEN;
	le->args[0] = arg0;
	le->args[1] = arg1;
	le->args[2] = file_size;
	le->nr_args = 3;
	le->flags = flags;
	logptr++;
	logcount++;
	if (logptr >= LOGSIZE)
		logptr = 0;
}

void
logdump(void)
{
	FILE	*logopsf;
	int	i, count, down;
	struct log_entry	*lp;

	prt("LOG DUMP (%d total operations):\n", logcount);

	logopsf = fopen(opsfile, "w");
	if (!logopsf)
		prterr(opsfile);

	if (logcount < LOGSIZE) {
		i = 0;
		count = logcount;
	} else {
		i = logptr;
		count = LOGSIZE;
	}
	for ( ; count > 0; count--) {
		bool overlap, overlap2;
		int opnum;

		opnum = i+1 + (logcount/LOGSIZE)*LOGSIZE;
		prt("%d(%3d mod 256): ", opnum, opnum%256);
		lp = &oplog[i];

		overlap = badoff >= lp->args[0] &&
			  badoff < lp->args[0] + lp->args[1];

		if (lp->flags & FL_SKIPPED) {
			prt("SKIPPED (no operation)");
			goto skipped;
		}

		switch (lp->operation) {
		case OP_MAPREAD:
			prt("MAPREAD  0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t***RRRR***");
			break;
		case OP_MAPWRITE:
			prt("MAPWRITE 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******WWWW");
			break;
		case OP_READ:
		case OP_READ_DONTCACHE:
			prt("READ     0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t***RRRR***");
			break;
		case OP_WRITE_DONTCACHE:
		case OP_WRITE_ATOMIC:
		case OP_WRITE:
			prt("WRITE    0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (lp->args[0] > lp->args[2])
				prt(" HOLE");
			else if (lp->args[0] + lp->args[1] > lp->args[2])
				prt(" EXTEND");
			overlap = (badoff >= lp->args[0] ||
				   badoff >=lp->args[2]) &&
				  badoff < lp->args[0] + lp->args[1];
			if (overlap)
				prt("\t***WWWW");
			break;
		case OP_TRUNCATE:
			down = lp->args[1] < lp->args[2];
			prt("TRUNCATE %s\tfrom 0x%x to 0x%x",
			    down ? "DOWN" : "UP", lp->args[2], lp->args[1]);
			overlap = badoff >= lp->args[1 + !down] &&
				  badoff < lp->args[1 + !!down];
			if (overlap)
				prt("\t******WWWW");
			break;
		case OP_FALLOCATE:
			/* 0: offset 1: length 2: where alloced */
			prt("FALLOC   0x%x thru 0x%x\t(0x%x bytes) ",
				lp->args[0], lp->args[0] + lp->args[1],
				lp->args[1]);
			if (lp->args[0] + lp->args[1] <= lp->args[2])
				prt("INTERIOR");
			else if (lp->flags & FL_KEEP_SIZE)
				prt("PAST_EOF");
			else
				prt("EXTENDING");
			if (overlap)
				prt("\t******FFFF");
			break;
		case OP_PUNCH_HOLE:
			prt("PUNCH    0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******PPPP");
			break;
		case OP_ZERO_RANGE:
			prt("ZERO     0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******ZZZZ");
			break;
		case OP_COLLAPSE_RANGE:
			prt("COLLAPSE 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******CCCC");
			break;
		case OP_INSERT_RANGE:
			prt("INSERT 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******IIII");
			break;
		case OP_EXCHANGE_RANGE:
			prt("XCHG 0x%x thru 0x%x\t(0x%x bytes) to 0x%x thru 0x%x",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1],
			    lp->args[2], lp->args[2] + lp->args[1] - 1);
			overlap2 = badoff >= lp->args[2] &&
				  badoff < lp->args[2] + lp->args[1];
			if (overlap && overlap2)
				prt("\tXXXX**XXXX");
			else if (overlap)
				prt("\tXXXX******");
			else if (overlap2)
				prt("\t******XXXX");
			break;
		case OP_CLONE_RANGE:
			prt("CLONE 0x%x thru 0x%x\t(0x%x bytes) to 0x%x thru 0x%x",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1],
			    lp->args[2], lp->args[2] + lp->args[1] - 1);
			overlap2 = badoff >= lp->args[2] &&
				  badoff < lp->args[2] + lp->args[1];
			if (overlap && overlap2)
				prt("\tJJJJ**JJJJ");
			else if (overlap)
				prt("\tJJJJ******");
			else if (overlap2)
				prt("\t******JJJJ");
			break;
		case OP_DEDUPE_RANGE:
			prt("DEDUPE 0x%x thru 0x%x\t(0x%x bytes) to 0x%x thru 0x%x",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1],
			    lp->args[2], lp->args[2] + lp->args[1] - 1);
			overlap2 = badoff >= lp->args[2] &&
				  badoff < lp->args[2] + lp->args[1];
			if (overlap && overlap2)
				prt("\tBBBB**BBBB");
			else if (overlap)
				prt("\tBBBB******");
			else if (overlap2)
				prt("\t******BBBB");
			break;
		case OP_COPY_RANGE:
			prt("COPY 0x%x thru 0x%x\t(0x%x bytes) to 0x%x thru 0x%x",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1],
			    lp->args[2], lp->args[2] + lp->args[1] - 1);
			overlap2 = badoff >= lp->args[2] &&
				  badoff < lp->args[2] + lp->args[1];
			if (overlap && overlap2)
				prt("\tEEEE**EEEE");
			else if (overlap)
				prt("\tEEEE******");
			else if (overlap2)
				prt("\t******EEEE");
			break;
		case OP_FSYNC:
			prt("FSYNC");
			break;
		default:
			prt("BOGUS LOG ENTRY (operation code = %d)!",
			    lp->operation);
			continue;
		}

	    skipped:
		if (lp->flags & FL_CLOSE_OPEN)
			prt("\n\t\tCLOSE/OPEN");
		prt("\n");
		i++;
		if (i == LOGSIZE)
			i = 0;

		if (logopsf) {
			int j;

			if (lp->flags & FL_SKIPPED)
				fprintf(logopsf, "skip ");
			fprintf(logopsf, "%s", op_name(lp->operation));
			for (j = 0; j < lp->nr_args; j++)
				fprintf(logopsf, " 0x%x", lp->args[j]);
			if (lp->flags & FL_KEEP_SIZE)
				fprintf(logopsf, " keep_size");
			if (lp->flags & FL_CLOSE_OPEN)
				fprintf(logopsf, " close_open");
			if (lp->flags & FL_UNSHARE)
				fprintf(logopsf, " unshare");
			if (overlap)
				fprintf(logopsf, " *");
			fprintf(logopsf, "\n");
		}
	}

	if (logopsf) {
		if (fclose(logopsf) != 0)
			prterr(opsfile);
		else
			prt("Log of operations saved to \"%s\"; "
			    "replay with --replay-ops\n",
			    opsfile);
	}
}


void
save_buffer(char *buffer, off_t bufferlength, int fd)
{
	off_t ret;
	ssize_t byteswritten;

	if (fd <= 0 || bufferlength == 0)
		return;

	if (bufferlength > SSIZE_MAX) {
		prt("fsx flaw: overflow in save_buffer\n");
		exit(67);
	}
	if (lite) {
		off_t size_by_seek = lseek(fd, (off_t)0, SEEK_END);
		if (size_by_seek == (off_t)-1)
			prterr("save_buffer: lseek eof");
		else if (bufferlength > size_by_seek) {
			warn("save_buffer: .fsxgood file too short... will save 0x%llx bytes instead of 0x%llx\n", (unsigned long long)size_by_seek,
			     (unsigned long long)bufferlength);
			bufferlength = size_by_seek;
		}
	}

	ret = lseek(fd, (off_t)0, SEEK_SET);
	if (ret == (off_t)-1)
		prterr("save_buffer: lseek 0");
	
	byteswritten = write(fd, buffer, (size_t)bufferlength);
	if (byteswritten != bufferlength) {
		if (byteswritten == -1)
			prterr("save_buffer write");
		else
			warn("save_buffer: short write, 0x%x bytes instead of 0x%llx\n",
			     (unsigned)byteswritten,
			     (unsigned long long)bufferlength);
	}
}


void
report_failure(int status)
{
	logdump();
	
	if (fsxgoodfd) {
		if (good_buf) {
			save_buffer(good_buf, file_size, fsxgoodfd);
			prt("Correct content saved for comparison\n");
			prt("(maybe hexdump \"%s\" vs \"%s\")\n",
			    fname, goodfile);
		}
		close(fsxgoodfd);
	}
	exit(status);
}


#define short_at(cp) ((unsigned short)((*((unsigned char *)(cp)) << 8) | \
				        *(((unsigned char *)(cp)) + 1)))

void
mark_log(void)
{
	char command[256];
	int ret;

	snprintf(command, 256, "dmsetup message %s 0 mark %s.mark%d", logdev,
		 bname, mark_nr);
	ret = system(command);
	if (ret) {
		prterr("dmsetup mark failed");
		exit(211);
	}
}

void
dump_fsync_buffer(void)
{
	char fname_buffer[PATH_MAX];
	int good_fd;

	if (!good_buf)
		return;

	snprintf(fname_buffer, sizeof(fname_buffer), "%s%s.mark%d", dname,
		 bname, mark_nr);
	good_fd = open(fname_buffer, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (good_fd < 0) {
		prterr(fname_buffer);
		exit(212);
	}

	save_buffer(good_buf, file_size, good_fd);
	close(good_fd);
	prt("Dumped fsync buffer to %s\n", fname_buffer + dirpath);
}

void
check_buffers(char *buf, unsigned offset, unsigned size)
{
	unsigned char c, t;
	unsigned i = 0;
	unsigned n = 0;
	unsigned op = 0;
	unsigned bad = 0;

	if (memcmp(good_buf + offset, buf, size) != 0) {
		prt("READ BAD DATA: offset = 0x%x, size = 0x%x, fname = %s\n",
		    offset, size, fname);
		prt("%-10s  %-6s  %-6s  %s\n", "OFFSET", "GOOD", "BAD", "RANGE");
		while (size > 0) {
			c = good_buf[offset];
			t = buf[i];
			if (c != t) {
			        if (n < 16) {
					bad = short_at(&buf[i]);
				        prt("0x%-8x  0x%04x  0x%04x  0x%x\n",
					    offset,
					    short_at(&good_buf[offset]), bad,
					    n);
					op = buf[offset & 1 ? i+1 : i];
					if (op)
						prt("operation# (mod 256) for "
						  "the bad data may be %u\n",
						((unsigned)op & 0xff));
					else
						prt("operation# (mod 256) for "
						  "the bad data unknown, check"
						  " HOLE and EXTEND ops\n");
				}
				n++;
				badoff = offset;
			}
			offset++;
			i++;
			size--;
		}
		report_failure(110);
	}
}


void
check_size(void)
{
	struct stat	statbuf;
	off_t	size_by_seek;

	if (fstat(fd, &statbuf)) {
		prterr("check_size: fstat");
		statbuf.st_size = -1;
	}
	size_by_seek = lseek(fd, (off_t)0, SEEK_END);
	if (file_size != statbuf.st_size || file_size != size_by_seek) {
		prt("Size error: expected 0x%llx stat 0x%llx seek 0x%llx\n",
		    (unsigned long long)file_size,
		    (unsigned long long)statbuf.st_size,
		    (unsigned long long)size_by_seek);
		report_failure(120);
	}
}


void
check_trunc_hack(void)
{
	struct stat statbuf;
	off_t offset = file_size + (off_t)100000;

	if (ftruncate(fd, file_size))
		goto ftruncate_err;
	if (ftruncate(fd, offset))
		goto ftruncate_err;
	fstat(fd, &statbuf);
	if (statbuf.st_size != offset) {
		prt("no extend on truncate! not posix!\n");
		exit(130);
	}
	if (ftruncate(fd, file_size)) {
ftruncate_err:
		prterr("check_trunc_hack: ftruncate");
		exit(131);
	}
}

void
doflush(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	char    *p;

	if (o_direct == O_DIRECT)
		return;

	pg_offset = offset & mmap_mask;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE,
			      MAP_FILE | MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
		prterr("doflush: mmap");
		report_failure(202);
	}
	if (msync(p, map_size, MS_INVALIDATE) != 0) {
		prterr("doflush: msync");
		report_failure(203);
	}
	if (munmap(p, map_size) != 0) {
		prterr("doflush: munmap");
		report_failure(204);
	}
}

void
doread(unsigned offset, unsigned size, int flags)
{
	unsigned iret;

	offset -= offset % readbdy;
	if (o_direct)
		size -= size % readbdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount && !o_direct)
			prt("skipping zero size read\n");
		log4(OP_READ, offset, size, FL_SKIPPED);
		return;
	}
	if (size + offset > file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping seek/read past end of file\n");
		log4(OP_READ, offset, size, FL_SKIPPED);
		return;
	}

	log4(OP_READ, offset, size, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0)  ||
		(debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lld read\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);
	iret = fsxread(fd, temp_buf, size, offset, flags);
	if (iret != size) {
		if (iret == -1)
			prterr("doread: read");
		else
			prt("short read: 0x%x bytes instead of 0x%x\n",
			    iret, size);
		report_failure(141);
	}
	check_buffers(temp_buf, offset, size);
}

void
check_eofpage(char *s, unsigned offset, char *p, int size)
{
	unsigned long last_page, should_be_zero;

	if (offset + size <= (file_size & ~page_mask))
		return;
	/*
	 * we landed in the last page of the file
	 * test to make sure the VM system provided 0's 
	 * beyond the true end of the file mapping
	 * (as required by mmap def in 1996 posix 1003.1)
	 */
	last_page = ((unsigned long)p + (offset & page_mask) + size) & ~page_mask;

	for (should_be_zero = last_page + (file_size & page_mask);
	     should_be_zero < last_page + page_size;
	     should_be_zero++)
		if (*(char *)should_be_zero) {
			prt("Mapped %s: non-zero data past EOF (0x%llx) page offset 0x%x is 0x%04x\n",
			    s, file_size - 1, should_be_zero & page_mask,
			    short_at(should_be_zero));
			report_failure(205);
		}
}

void
check_contents(void)
{
	static char *check_buf;
	unsigned offset = 0;
	unsigned size = file_size;
	unsigned map_offset;
	unsigned map_size;
	char *p;
	unsigned iret;

	if (!check_buf) {
		check_buf = (char *) malloc(maxfilelen + writebdy);
		assert(check_buf != NULL);
		check_buf = round_ptr_up(check_buf, writebdy, 0);
		memset(check_buf, '\0', maxfilelen);
	}

	if (o_direct)
		size -= size % readbdy;
	if (size == 0)
		return;

	iret = fsxread(fd, check_buf, size, offset, 0);
	if (iret != size) {
		if (iret == -1)
			prterr("check_contents: read");
		else
			prt("short check read: 0x%x bytes instead of 0x%x\n",
			    iret, size);
		report_failure(141);
	}
	check_buffers(check_buf, offset, size);

	/* Map eof page, check it */
	map_offset = size - (size & PAGE_MASK);
	if (map_offset == size)
		map_offset -= PAGE_SIZE;
	map_size  = size - map_offset;

	p = mmap(0, map_size, PROT_READ, MAP_SHARED, fd, map_offset);
	if (p == MAP_FAILED) {
	        prterr("check_contents: mmap");
		report_failure(190);
	}
	check_eofpage("check_contents", map_offset, p, map_size);

	if (munmap(p, map_size) != 0) {
		prterr("check_contents: munmap");
		report_failure(191);
	}
}

void
domapread(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	char    *p;

	offset -= offset % readbdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero size read\n");
		log4(OP_MAPREAD, offset, size, FL_SKIPPED);
		return;
	}
	if (size + offset > file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping seek/read past end of file\n");
		log4(OP_MAPREAD, offset, size, FL_SKIPPED);
		return;
	}

	log4(OP_MAPREAD, offset, size, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lld mapread\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);

	pg_offset = offset & PAGE_MASK;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ, MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
	        prterr("domapread: mmap");
		report_failure(190);
	}
	memcpy(temp_buf, p + pg_offset, size);

	check_eofpage("Read", offset, p, size);

	if (munmap(p, map_size) != 0) {
		prterr("domapread: munmap");
		report_failure(191);
	}

	check_buffers(temp_buf, offset, size);
}


void
gendata(char *original_buf, char *good_buf, unsigned offset, unsigned size)
{
	while (size--) {
		if (filldata) {
			good_buf[offset] = filldata;
		} else {
			good_buf[offset] = testcalls % 256;
			if (offset % 2)
				good_buf[offset] += original_buf[offset];
		}
		offset++;
	}
}

/*
 * Pollute the EOF page with data beyond EOF prior to size change operations.
 * This provides additional test coverage for partial EOF block/page zeroing.
 * If the upcoming operation does not correctly zero, incorrect file data will
 * be detected.
 */
void
pollute_eofpage(unsigned int maxoff)
{
	unsigned offset = file_size;
	unsigned pg_offset;
	unsigned write_size;
	char    *p;

	/*
	 * Nothing to do if pollution disabled or we're simulating. Simulation
	 * only tracks file size updates and skips syscalls, so we don't want to
	 * inject file data that won't be zeroed.
	 */
	if (!pollute_eof || testcalls <= simulatedopcount)
		return;

	/* write up to specified max or the end of the eof page */
	pg_offset = offset & mmap_mask;
	write_size = MIN(PAGE_SIZE - pg_offset, maxoff - offset);

	if (!pg_offset)
		return;

	if (!quiet &&
	    ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug &&
	     (monitorstart == -1 ||
	     (offset + write_size > monitorstart &&
	      (monitorend == -1 || offset <= monitorend)))))) {
		prt("%lld pollute_eof\t0x%x thru\t0x%x\t(0x%x bytes)\n",
			testcalls, offset, offset + write_size - 1, write_size);
	}

	if ((p = (char *)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
			      MAP_FILE | MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == MAP_FAILED) {
		prterr("pollute_eofpage: mmap");
		return;
	}

	/*
	 * Write to a range just past EOF of the test file. Do not update the
	 * good buffer because the upcoming operation is expected to zero this
	 * range of the file.
	 */
	gendata(original_buf, p, pg_offset, write_size);

	if (munmap(p, PAGE_SIZE) != 0)
		prterr("pollute_eofpage: munmap");
}

/*
 * Helper to update the tracked file size. If the offset begins beyond current
 * EOF, zero the range from EOF to offset in the good buffer.
 */
void
update_file_size(unsigned offset, unsigned size)
{
	if (offset > file_size) {
		pollute_eofpage(offset + size);
		memset(good_buf + file_size, '\0', offset - file_size);
	}
	file_size = offset + size;
}

static int is_power_of_2(unsigned n) {
	return ((n & (n - 1)) == 0);
}

/*
 * Round down n to nearest power of 2.
 * If n is already a power of 2, return n;
 */
static int rounddown_pow_of_2(int n) {
	int i = 0;

	if (is_power_of_2(n))
		return n;

	for (; (1 << i) < n; i++);

	return 1 << (i - 1);
}

void
dowrite(unsigned offset, unsigned size, int flags)
{
	unsigned iret;

	offset -= offset % writebdy;
	if (o_direct)
		size -= size % writebdy;
	if (flags & RWF_ATOMIC) {
		/* atomic write len must be between awu_min and awu_max */
		if (size < awu_min)
			size = awu_min;
		if (size > awu_max)
			size = awu_max;

		/* atomic writes need power-of-2 sizes */
		size = rounddown_pow_of_2(size);

		/* atomic writes need naturally aligned offsets */
		offset -= offset % size;

		/* Skip the write if we are crossing max filesize */
		if ((offset + size) > maxfilelen) {
			if (!quiet && testcalls > simulatedopcount)
				prt("skipping atomic write past maxfilelen\n");
			log4(OP_WRITE_ATOMIC, offset, size, FL_SKIPPED);
			return;
		}
	}
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount && !o_direct)
			prt("skipping zero size write\n");
		log4(OP_WRITE, offset, size, FL_SKIPPED);
		return;
	}

	if (flags & RWF_ATOMIC)
		log4(OP_WRITE_ATOMIC, offset, size, FL_NONE);
	else
		log4(OP_WRITE, offset, size, FL_NONE);

	gendata(original_buf, good_buf, offset, size);
	if (offset + size > file_size) {
		update_file_size(offset, size);
		if (lite) {
			warn("Lite file size bug in fsx!");
			report_failure(149);
		}
	}

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lld write\t0x%x thru\t0x%x\t(0x%x bytes)\tdontcache=%d atomic_wr=%d\n", testcalls,
		    offset, offset + size - 1, size, (flags & RWF_DONTCACHE) != 0,
		    (flags & RWF_ATOMIC) != 0);
	iret = fsxwrite(fd, good_buf + offset, size, offset, flags);
	if (iret != size) {
		if (iret == -1)
			prterr("dowrite: write");
		else
			prt("short write: 0x%x bytes instead of 0x%x\n",
			    iret, size);
		report_failure(151);
	}
	if (do_fsync) {
		if (fsync(fd)) {
			prt("fsync() failed: %s\n", strerror(errno));
			report_failure(152);
		}
	}
	if (flush) {
		doflush(offset, size);
	}
}


void
domapwrite(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	off_t    cur_filesize;
	char    *p;

	offset -= offset % writebdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero size write\n");
		log4(OP_MAPWRITE, offset, size, FL_SKIPPED);
		return;
	}
	cur_filesize = file_size;

	log4(OP_MAPWRITE, offset, size, FL_NONE);

	gendata(original_buf, good_buf, offset, size);
	if (offset + size > file_size) {
		update_file_size(offset, size);
		if (lite) {
			warn("Lite file size bug in fsx!");
			report_failure(200);
		}
	}

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lld mapwrite\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);

	if (file_size > cur_filesize) {
	        if (ftruncate(fd, file_size) == -1) {
		        prterr("domapwrite: ftruncate");
			exit(201);
		}
	}
	pg_offset = offset & PAGE_MASK;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE,
			      MAP_FILE | MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
	        prterr("domapwrite: mmap");
		report_failure(202);
	}
	memcpy(p + pg_offset, good_buf + offset, size);
	if (msync(p, map_size, MS_SYNC) != 0) {
		prterr("domapwrite: msync");
		report_failure(203);
	}

	check_eofpage("Write", offset, p, size);

	if (munmap(p, map_size) != 0) {
		prterr("domapwrite: munmap");
		report_failure(204);
	}
}


void
dotruncate(unsigned size)
{
	int oldsize = file_size;

	size -= size % truncbdy;
	if (size > biggest) {
		biggest = size;
		if (!quiet && testcalls > simulatedopcount)
			prt("truncating to largest ever: 0x%x\n", size);
	}

	log4(OP_TRUNCATE, 0, size, FL_NONE);

	/* pollute the current EOF before a truncate down */
	if (size < file_size)
		pollute_eofpage(maxfilelen);
	update_file_size(size, 0);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      size <= monitorend)))
		prt("%lld trunc\tfrom 0x%x to 0x%x\n", testcalls, oldsize,
				size);
	if (ftruncate(fd, (off_t)size) == -1) {
	        prt("ftruncate1: %x\n", size);
		prterr("dotruncate: ftruncate");
		report_failure(160);
	}
}

#ifdef FALLOC_FL_PUNCH_HOLE
void
do_punch_hole(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int max_offset = 0;
	int max_len = 0;
	int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length punch hole\n");
		log4(OP_PUNCH_HOLE, offset, length, FL_SKIPPED);
		return;
	}

	if (file_size <= (loff_t)offset) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping hole punch off the end of the file\n");
		log4(OP_PUNCH_HOLE, offset, length, FL_SKIPPED);
		return;
	}

	end_offset = offset + length;

	log4(OP_PUNCH_HOLE, offset, length, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lld punch\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("punch hole: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_punch_hole: fallocate");
		report_failure(161);
	}


	max_offset = offset < file_size ? offset : file_size;
	max_len = max_offset + length <= file_size ? length :
			file_size - max_offset;
	memset(good_buf + max_offset, '\0', max_len);
}

#else
void
do_punch_hole(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef FALLOC_FL_ZERO_RANGE
void
do_zero_range(unsigned offset, unsigned length, int keep_size)
{
	unsigned end_offset;
	int mode = FALLOC_FL_ZERO_RANGE;

	if (keep_size)
		mode |= FALLOC_FL_KEEP_SIZE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length zero range\n");
		log4(OP_ZERO_RANGE, offset, length, FL_SKIPPED |
		     (keep_size ? FL_KEEP_SIZE : FL_NONE));
		return;
	}

	end_offset = keep_size ? 0 : offset + length;

	if (end_offset > biggest) {
		biggest = end_offset;
		if (!quiet && testcalls > simulatedopcount)
			prt("zero_range to largest ever: 0x%x\n", end_offset);
	}

	/*
	 * last arg matches fallocate string array index in logdump:
	 * 	0: allocate past EOF
	 * 	1: extending prealloc
	 * 	2: interior prealloc
	 */
	log4(OP_ZERO_RANGE, offset, length,
	     keep_size ? FL_KEEP_SIZE : FL_NONE);

	if (!keep_size && end_offset > file_size)
		update_file_size(offset, length);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lld zero\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("zero range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_zero_range: fallocate");
		report_failure(161);
	}

	memset(good_buf + offset, '\0', length);
}

#else
void
do_zero_range(unsigned offset, unsigned length, int keep_size)
{
	return;
}
#endif

#ifdef FALLOC_FL_COLLAPSE_RANGE
void
do_collapse_range(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int mode = FALLOC_FL_COLLAPSE_RANGE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length collapse range\n");
		log4(OP_COLLAPSE_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	end_offset = offset + length;
	if ((loff_t)end_offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping collapse range behind EOF\n");
		log4(OP_COLLAPSE_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	log4(OP_COLLAPSE_RANGE, offset, length, FL_NONE);

	/* pollute current eof before collapse truncates down */
	pollute_eofpage(maxfilelen);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lld collapse\tfrom 0x%x to 0x%x, (0x%x bytes)\n",
				testcalls, offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("collapse range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_collapse_range: fallocate");
		report_failure(161);
	}

	memmove(good_buf + offset, good_buf + end_offset,
		file_size - end_offset);
	file_size -= length;
}

#else
void
do_collapse_range(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef FALLOC_FL_INSERT_RANGE
void
do_insert_range(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int mode = FALLOC_FL_INSERT_RANGE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length insert range\n");
		log4(OP_INSERT_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping insert range behind EOF\n");
		log4(OP_INSERT_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	log4(OP_INSERT_RANGE, offset, length, FL_NONE);

	/* pollute current eof before insert truncates up */
	pollute_eofpage(maxfilelen);

	if (testcalls <= simulatedopcount)
		return;

	end_offset = offset + length;
	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lld insert\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("insert range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_insert_range: fallocate");
		report_failure(161);
	}

	memmove(good_buf + end_offset, good_buf + offset,
		file_size - offset);
	memset(good_buf + offset, '\0', length);
	file_size += length;
}

#else
void
do_insert_range(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef XFS_IOC_EXCHANGE_RANGE
int
test_exchange_range(void)
{
	struct xfs_exchange_range	fsr = {
		.file1_fd = fd,
		.flags = XFS_EXCHANGE_RANGE_DRY_RUN,
	};
	int ret, e;

	ret = ioctl(fd, XFS_IOC_EXCHANGE_RANGE, &fsr);
	e = ret < 0 ? errno : 0;
	if (e == EOPNOTSUPP || errno == ENOTTY) {
		if (!quiet)
			fprintf(stderr,
				"main: filesystem does not support "
				"exchange range, disabling!\n");
		return 0;
	}

	return 1;
}

void
do_exchange_range(unsigned offset, unsigned length, unsigned dest)
{
	struct xfs_exchange_range	fsr = {
		.file1_fd = fd,
		.file1_offset = offset,
		.file2_offset = dest,
		.length = length,
	};
	void *p;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length exchange range\n");
		log5(OP_EXCHANGE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size || (loff_t)dest >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping exchange range behind EOF\n");
		log5(OP_EXCHANGE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	p = malloc(length);
	if (!p) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping exchange range due to ENOMEM\n");
		log5(OP_EXCHANGE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	log5(OP_EXCHANGE_RANGE, offset, length, dest, FL_NONE);

	if (testcalls <= simulatedopcount)
		goto out_free;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		       dest <= monitorstart || dest + length <= monitorend))) {
		prt("%lu swap\tfrom 0x%x to 0x%x, (0x%x bytes) at 0x%x\n",
			testcalls, offset, offset+length, length, dest);
	}

	if (ioctl(fd, XFS_IOC_EXCHANGE_RANGE, &fsr) == -1) {
		prt("exchange range: 0x%x to 0x%x at 0x%x\n", offset,
				offset + length, dest);
		prterr("do_exchange_range: XFS_IOC_EXCHANGE_RANGE");
		report_failure(161);
		goto out_free;
	}

	memcpy(p, good_buf + offset, length);
	memcpy(good_buf + offset, good_buf + dest, length);
	memcpy(good_buf + dest, p, length);
out_free:
	free(p);
}

#else
int
test_exchange_range(void)
{
	return 0;
}

void
do_exchange_range(unsigned offset, unsigned length, unsigned dest)
{
	return;
}
#endif

#ifdef FICLONERANGE
int
test_clone_range(void)
{
	struct file_clone_range	fcr = {
		.src_fd = fd,
	};

	if (ioctl(fd, FICLONERANGE, &fcr) &&
	    (errno == EOPNOTSUPP || errno == ENOTTY)) {
		if (!quiet)
			fprintf(stderr,
				"main: filesystem does not support "
				"clone range, disabling!\n");
		return 0;
	}

	return 1;
}

void
do_clone_range(unsigned offset, unsigned length, unsigned dest)
{
	struct file_clone_range	fcr = {
		.src_fd = fd,
		.src_offset = offset,
		.src_length = length,
		.dest_offset = dest,
	};

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length clone range\n");
		log5(OP_CLONE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping clone range behind EOF\n");
		log5(OP_CLONE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if (dest + length > biggest) {
		biggest = dest + length;
		if (!quiet && testcalls > simulatedopcount)
			prt("cloning to largest ever: 0x%x\n", dest + length);
	}

	log5(OP_CLONE_RANGE, offset, length, dest, FL_NONE);

	if (dest + length > file_size)
		update_file_size(dest, length);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		       dest <= monitorstart || dest + length <= monitorend))) {
		prt("%lu clone\tfrom 0x%x to 0x%x, (0x%x bytes) at 0x%x\n",
			testcalls, offset, offset+length, length, dest);
	}

	if (ioctl(fd, FICLONERANGE, &fcr) == -1) {
		prt("clone range: 0x%x to 0x%x at 0x%x\n", offset,
				offset + length, dest);
		prterr("do_clone_range: FICLONERANGE");
		report_failure(161);
	}

	memcpy(good_buf + dest, good_buf + offset, length);
}

#else
int
test_clone_range(void)
{
	return 0;
}

void
do_clone_range(unsigned offset, unsigned length, unsigned dest)
{
	return;
}
#endif

#ifdef FIDEDUPERANGE
int
test_dedupe_range(void)
{
	struct file_dedupe_range *fdr;
	off_t new_len;
	int error;
	int ret = 1;

	/* Alloc memory */
	fdr = calloc(sizeof(struct file_dedupe_range_info) +
		     sizeof(struct file_dedupe_range), 1);
	if (!fdr) {
		prterr("do_dedupe_range: malloc");
		report_failure(161);
	}

	/* Make sure we have at least two blocks */
	new_len = block_size * 2;
	if (file_size < new_len && ftruncate(fd, new_len)) {
		warn("main: ftruncate");
		exit(132);
	}

	/* Try to dedupe them */
	fdr->src_length = block_size;
	fdr->dest_count = 1;
	fdr->info[0].dest_fd = fd;
	fdr->info[0].dest_offset = block_size;

	if (ioctl(fd, FIDEDUPERANGE, fdr))
		error = errno;
	else if (fdr->info[0].status < 0)
		error = -fdr->info[0].status;
	else
		error = 0;

	/* Older kernels may return EINVAL... */
	if (error == EOPNOTSUPP || error == ENOTTY || error == EINVAL) {
		if (!quiet)
			fprintf(stderr,
				"main: filesystem does not support "
				"dedupe range, disabling!\n");
		ret = 0;
	}

	/* Put the file back the way it was. */
	if (file_size < new_len && ftruncate(fd, file_size)) {
		warn("main: ftruncate");
		exit(132);
	}

	free(fdr);
	return ret;
}

void
do_dedupe_range(unsigned offset, unsigned length, unsigned dest)
{
	struct file_dedupe_range *fdr;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length dedupe range\n");
		log5(OP_DEDUPE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping dedupe range behind EOF\n");
		log5(OP_DEDUPE_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	log5(OP_DEDUPE_RANGE, offset, length, dest, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		       dest <= monitorstart || dest + length <= monitorend))) {
		prt("%lu dedupe\tfrom 0x%x to 0x%x, (0x%x bytes) at 0x%x\n",
			testcalls, offset, offset+length, length, dest);
	}

	/* Alloc memory */
	fdr = calloc(sizeof(struct file_dedupe_range_info) +
		     sizeof(struct file_dedupe_range), 1);
	if (!fdr) {
		prterr("do_dedupe_range: malloc");
		report_failure(161);
	}

	/* Dedupe data blocks */
	fdr->src_offset = offset;
	fdr->src_length = length;
	fdr->dest_count = 1;
	fdr->info[0].dest_fd = fd;
	fdr->info[0].dest_offset = dest;

	if (ioctl(fd, FIDEDUPERANGE, fdr) == -1) {
		prt("dedupe range: 0x%x to 0x%x at 0x%x\n", offset,
				offset + length, dest);
		prterr("do_dedupe_range(0): FIDEDUPERANGE");
		report_failure(161);
	} else if (fdr->info[0].status < 0) {
		errno = -fdr->info[0].status;
		prt("dedupe range: 0x%x to 0x%x at 0x%x\n", offset,
				offset + length, dest);
		prterr("do_dedupe_range(1): FIDEDUPERANGE");
		report_failure(161);
	}

	free(fdr);
}

#else
int
test_dedupe_range(void)
{
	return 0;
}

void
do_dedupe_range(unsigned offset, unsigned length, unsigned dest)
{
	return;
}
#endif

int test_atomic_writes(void) {
	int ret;
	struct statx stx;

	if (o_direct != O_DIRECT) {
		if (!quiet)
			fprintf(stderr, "main: atomic writes need O_DIRECT (-Z), "
					"disabling!\n");
		return 0;
	}

	ret = xfstests_statx(AT_FDCWD, fname, 0, STATX_WRITE_ATOMIC, &stx);
	if (ret < 0) {
		fprintf(stderr, "main: Statx failed with %d."
			" Failed to determine atomic write limits, "
			" disabling!\n", ret);
		return 0;
	}

	if (stx.stx_attributes & STATX_ATTR_WRITE_ATOMIC &&
	    stx.stx_atomic_write_unit_min > 0) {
		awu_min = stx.stx_atomic_write_unit_min;
		awu_max = stx.stx_atomic_write_unit_max;
		return 1;
	}

	if (!quiet)
		fprintf(stderr, "main: IO Stack does not support "
				"atomic writes, disabling!\n");
	return 0;
}

#ifdef HAVE_COPY_FILE_RANGE
int
test_copy_range(void)
{
	loff_t o1 = 0, o2 = 1;

	if (syscall(__NR_copy_file_range, fd, &o1, fd, &o2, 1, 0) == -1 &&
	    (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTTY)) {
		if (!quiet)
			fprintf(stderr,
				"main: filesystem does not support "
				"copy range, disabling!\n");
		return 0;
	}

	return 1;
}

void
do_copy_range(unsigned offset, unsigned length, unsigned dest)
{
	loff_t o1, o2;
	size_t olen;
	ssize_t nr;
	int tries = 0;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length copy range\n");
		log5(OP_COPY_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping copy range behind EOF\n");
		log5(OP_COPY_RANGE, offset, length, dest, FL_SKIPPED);
		return;
	}

	if (dest + length > biggest) {
		biggest = dest + length;
		if (!quiet && testcalls > simulatedopcount)
			prt("copying to largest ever: 0x%x\n", dest + length);
	}

	log5(OP_COPY_RANGE, offset, length, dest, FL_NONE);

	if (dest + length > file_size)
		update_file_size(dest, length);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		       dest <= monitorstart || dest + length <= monitorend))) {
		prt("%lu copy\tfrom 0x%x to 0x%x, (0x%x bytes) at 0x%x\n",
			testcalls, offset, offset+length, length, dest);
	}

	o1 = offset;
	o2 = dest;
	olen = length;

	while (olen > 0) {
		nr = syscall(__NR_copy_file_range, fd, &o1, fd, &o2, olen, 0);
		if (nr < 0) {
			if (errno != EAGAIN || tries++ >= 300)
				break;
		} else if (nr > olen) {
			prt("copy range: 0x%x to 0x%x at 0x%x\n", offset,
					offset + length, dest);
			prt("do_copy_range: asked %u, copied %u??\n",
					olen, nr);
			report_failure(161);
		} else if (nr > 0)
			olen -= nr;
	}
	if (nr < 0) {
		prt("copy range: 0x%x to 0x%x at 0x%x\n", offset,
				offset + length, dest);
		prterr("do_copy_range:");
		report_failure(161);
	}

	memcpy(good_buf + dest, good_buf + offset, length);
}

#else
int
test_copy_range(void)
{
	return 0;
}

void
do_copy_range(unsigned offset, unsigned length, unsigned dest)
{
	return;
}
#endif

#ifdef HAVE_LINUX_FALLOC_H
/* fallocate is basically a no-op unless extending, then a lot like a truncate */
void
do_preallocate(unsigned offset, unsigned length, int keep_size, int unshare)
{
	unsigned end_offset;
	enum opflags opflags = FL_NONE;
	int mode = 0;

	if (keep_size) {
		opflags |= FL_KEEP_SIZE;
		mode |= FALLOC_FL_KEEP_SIZE;
	}
#ifdef FALLOC_FL_UNSHARE_RANGE
	if (unshare) {
		opflags |= FL_UNSHARE;
		mode |= FALLOC_FL_UNSHARE_RANGE;
	}
#endif

        if (length == 0) {
                if (!quiet && testcalls > simulatedopcount)
                        prt("skipping zero length fallocate\n");
                log4(OP_FALLOCATE, offset, length, FL_SKIPPED | opflags);
                return;
        }

	end_offset = keep_size ? 0 : offset + length;

	if (end_offset > biggest) {
		biggest = end_offset;
		if (!quiet && testcalls > simulatedopcount)
			prt("fallocating to largest ever: 0x%x\n", end_offset);
	}

	/*
	 * last arg matches fallocate string array index in logdump:
	 * 	0: allocate past EOF
	 * 	1: extending prealloc
	 * 	2: interior prealloc
	 */
	log4(OP_FALLOCATE, offset, length, opflags);

	if (end_offset > file_size) {
		memset(good_buf + file_size, '\0', end_offset - file_size);
		update_file_size(offset, length);
	}

	if (testcalls <= simulatedopcount)
		return;
	
	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend)))
		prt("%lld falloc\tfrom 0x%x to 0x%x (0x%x bytes)\n", testcalls,
				offset, offset + length, length);
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
	        prt("fallocate: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_preallocate: fallocate");
		report_failure(161);
	}
}
#else
void
do_preallocate(unsigned offset, unsigned length, int keep_size, int unshare)
{
	return;
}
#endif

int
test_dontcache_io(void)
{
	char buf[4096];
	struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
	int ret, e;

	ret = preadv2(fd, &iov, 1, 0, RWF_DONTCACHE);
	e = ret < 0 ? errno : 0;
	if (e == EOPNOTSUPP) {
		if (!quiet)
			fprintf(stderr,
				"main: filesystem does not support "
				"dontcache IO, disabling!\n");
		return 0;
	}

	return 1;
}

void
writefileimage()
{
	ssize_t iret;

	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		prterr("writefileimage: lseek");
		report_failure(171);
	}
	iret = write(fd, good_buf, file_size);
	if ((off_t)iret != file_size) {
		if (iret == -1)
			prterr("writefileimage: write");
		else
			prt("short write: 0x%x bytes instead of 0x%llx\n",
			    iret, (unsigned long long)file_size);
		report_failure(172);
	}
	if (lite ? 0 : ftruncate(fd, file_size) == -1) {
	        prt("ftruncate2: %llx\n", (unsigned long long)file_size);
		prterr("writefileimage: ftruncate");
		report_failure(173);
	}
}


void
docloseopen(void)
{ 
	if (testcalls <= simulatedopcount)
		return;

	if (debug)
		prt("%lld close/open\n", testcalls);
	if (close(fd)) {
		prterr("docloseopen: close");
		report_failure(180);
	}
	if (system("echo 3 > /proc/sys/vm/drop_caches")) {
		prterr("docloseopen: drop_caches");
		report_failure(181);
	}
	fd = open(fname, O_RDWR|o_direct, 0);
	if (fd < 0) {
		prterr("docloseopen: open");
		report_failure(182);
	}
}

void
dofsync(void)
{
	int ret;

	if (testcalls <= simulatedopcount)
		return;
	if (debug)
		prt("%lld fsync\n", testcalls);
	log4(OP_FSYNC, 0, 0, 0);
	ret = fsync(fd);
	if (ret < 0) {
		prterr("dofsync");
		report_failure(210);
	}
	mark_log();
	dump_fsync_buffer();
	mark_nr++;
}

#define TRIM_OFF(off, size)			\
do {						\
	if (size)				\
		(off) %= (size);		\
	else					\
		(off) = 0;			\
} while (0)

#define TRIM_LEN(off, len, size)		\
do {						\
	if ((off) + (len) > (size))		\
		(len) = (size) - (off);		\
} while (0)

#define TRIM_OFF_LEN(off, len, size)		\
do {						\
	TRIM_OFF(off, size);			\
	TRIM_LEN(off, len, size);		\
} while (0)

void
cleanup(int sig)
{
	if (sig)
		prt("signal %d\n", sig);
	prt("testcalls = %lld\n", testcalls);
	exit(sig);
}

static int
op_args_count(int operation)
{
	switch (operation) {
	case OP_EXCHANGE_RANGE:
	case OP_CLONE_RANGE:
	case OP_DEDUPE_RANGE:
	case OP_COPY_RANGE:
		return 4;
	default:
		return 3;
	}
}

static int
read_op(struct log_entry *log_entry)
{
	char line[256];

	memset(log_entry, 0, sizeof(*log_entry));
	log_entry->operation = -1;

	while (log_entry->operation == -1) {
		char *str;
		int i;

		do {
			if (!fgets(line, sizeof(line), replayopsf)) {
				if (feof(replayopsf)) {
					replayopsf = NULL;
					return 0;
				}
				goto fail;
			}
			str = strtok(line, " \t\n");
		} while (!str || str[0] == '#');

		if (strcmp(str, "skip") == 0) {
			log_entry->flags |= FL_SKIPPED;
			str = strtok(NULL, " \t\n");
			if (!str)
				goto fail;
		}
		log_entry->operation = op_code(str);
		if (log_entry->operation == -1)
			goto fail;
		log_entry->nr_args = op_args_count(log_entry->operation);
		for (i = 0; i < log_entry->nr_args; i++) {
			char *end;

			str = strtok(NULL, " \t\n");
			if (!str)
				goto fail;
			log_entry->args[i] = strtoul(str, &end, 0);
			if (*end)
				goto fail;
		}
		while ((str = strtok(NULL, " \t\n"))) {
			if (strcmp(str, "keep_size") == 0)
				log_entry->flags |= FL_KEEP_SIZE;
			else if (strcmp(str, "close_open") == 0)
				log_entry->flags |= FL_CLOSE_OPEN;
			else if (strcmp(str, "unshare") == 0)
				log_entry->flags |= FL_UNSHARE;
			else if (strcmp(str, "*") == 0)
				;  /* overlap marker; ignore */
			else
				goto fail;
		}
	}
	return 1;

fail:
	fprintf(stderr, "%s: parse error\n", replayops);
	fclose(replayopsf);
	replayopsf = NULL;
	cleanup(100);  /* doesn't return */
	return 0;
}

static inline bool
range_overlaps(
	unsigned long	off0,
	unsigned long	off1,
	unsigned long	size)
{
	return llabs((unsigned long long)off1 - off0) < size;
}

static void generate_dest_range(bool bdy_align,
				unsigned long max_range_end,
				unsigned long *src_offset,
				unsigned long *size,
				unsigned long *dst_offset)
{
	int tries = 0;

	TRIM_OFF_LEN(*src_offset, *size, file_size);
	if (bdy_align) {
		*src_offset = rounddown_64(*src_offset, readbdy);
		if (o_direct)
			*size = rounddown_64(*size, readbdy);
	} else {
		*src_offset = rounddown_64(*src_offset, block_size);
		*size = rounddown_64(*size, block_size);
	}

	do {
		if (tries++ >= 30) {
			*size = 0;
			break;
		}
		*dst_offset = random();
		TRIM_OFF(*dst_offset, max_range_end);
		if (bdy_align)
			*dst_offset = rounddown_64(*dst_offset, writebdy);
		else
			*dst_offset = rounddown_64(*dst_offset, block_size);
	} while (range_overlaps(*src_offset, *dst_offset, *size) ||
		 *dst_offset + *size > max_range_end);
}

int
test(void)
{
	unsigned long	offset, offset2;
	unsigned long	size;
	unsigned long	rv;
	unsigned long	op;
	int		keep_size = 0;
	int		unshare = 0;

	if (simulatedopcount > 0 && testcalls == simulatedopcount)
		writefileimage();

	testcalls++;

	if (debugstart > 0 && testcalls >= debugstart)
		debug = 1;

	if (!quiet && testcalls < simulatedopcount && testcalls % 100000 == 0)
		prt("%lld...\n", testcalls);

	if (replayopsf) {
		struct log_entry log_entry;

		while (read_op(&log_entry)) {
			if (log_entry.flags & FL_SKIPPED) {
				log4(log_entry.operation,
				     log_entry.args[0], log_entry.args[1],
				     log_entry.flags);
				continue;
			}

			op = log_entry.operation;
			offset = log_entry.args[0];
			size = log_entry.args[1];
			offset2 = log_entry.args[2];
			closeopen = !!(log_entry.flags & FL_CLOSE_OPEN);
			keep_size = !!(log_entry.flags & FL_KEEP_SIZE);
			unshare = !!(log_entry.flags & FL_UNSHARE);
			goto have_op;
		}
		return 0;
	}

	rv = random();
	if (closeprob)
		closeopen = (rv >> 3) < (1 << 28) / closeprob;

	offset = random();
	offset2 = 0;
	size = maxoplen;
	if (randomoplen)
		size = random() % (maxoplen + 1);

	/* calculate appropriate op to run */
	if (lite)
		op = rv % OP_MAX_LITE;
	else if (!integrity)
		op = rv % OP_MAX_FULL;
	else
		op = rv % OP_MAX_INTEGRITY;

	switch(op) {
	case OP_TRUNCATE:
		if (!style)
			size = random() % maxfilelen;
		break;
	case OP_FALLOCATE:
		if (fallocate_calls && size) {
			if (keep_size_calls)
				keep_size = random() % 2;
			if (unshare_range_calls)
				unshare = random() % 2;
		}
		break;
	case OP_ZERO_RANGE:
		if (zero_range_calls && size && keep_size_calls)
			keep_size = random() % 2;
		break;
	case OP_CLONE_RANGE:
		generate_dest_range(false, maxfilelen, &offset, &size, &offset2);
		break;
	case OP_DEDUPE_RANGE:
		generate_dest_range(false, file_size, &offset, &size, &offset2);
		break;
	case OP_COPY_RANGE:
		generate_dest_range(true, maxfilelen, &offset, &size, &offset2);
		break;
	case OP_EXCHANGE_RANGE:
		generate_dest_range(false, file_size, &offset, &size, &offset2);
		break;
	}

have_op:

	switch (op) {
	case OP_MAPREAD:
		if (!mapped_reads)
			op = OP_READ;
		break;
	case OP_MAPWRITE:
		if (!mapped_writes)
			op = OP_WRITE;
		break;
	case OP_FALLOCATE:
		if (!fallocate_calls) {
			log4(OP_FALLOCATE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_PUNCH_HOLE:
		if (!punch_hole_calls) {
			log4(OP_PUNCH_HOLE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_ZERO_RANGE:
		if (!zero_range_calls) {
			log4(OP_ZERO_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_COLLAPSE_RANGE:
		if (!collapse_range_calls) {
			log4(OP_COLLAPSE_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_INSERT_RANGE:
		if (!insert_range_calls) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_EXCHANGE_RANGE:
		if (!exchange_range_calls) {
			log5(op, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_CLONE_RANGE:
		if (!clone_range_calls) {
			log5(op, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_DEDUPE_RANGE:
		if (!dedupe_range_calls) {
			log5(op, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_COPY_RANGE:
		if (!copy_range_calls) {
			log5(op, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_WRITE_ATOMIC:
		if (!do_atomic_writes) {
			log4(OP_WRITE_ATOMIC, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	}

	switch (op) {
	case OP_READ:
		TRIM_OFF_LEN(offset, size, file_size);
		doread(offset, size, 0);
		break;

	case OP_READ_DONTCACHE:
		TRIM_OFF_LEN(offset, size, file_size);
		if (dontcache_io)
			doread(offset, size, RWF_DONTCACHE);
		else
			doread(offset, size, 0);
		break;

	case OP_WRITE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		dowrite(offset, size, 0);
		break;

	case OP_WRITE_DONTCACHE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		if (dontcache_io)
			dowrite(offset, size, RWF_DONTCACHE);
		else
			dowrite(offset, size, 0);
		break;

	case OP_WRITE_ATOMIC:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		dowrite(offset, size, RWF_ATOMIC);
		break;

	case OP_MAPREAD:
		TRIM_OFF_LEN(offset, size, file_size);
		domapread(offset, size);
		break;

	case OP_MAPWRITE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		domapwrite(offset, size);
		break;

	case OP_TRUNCATE:
		dotruncate(size);
		break;

	case OP_FALLOCATE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		do_preallocate(offset, size, keep_size, unshare);
		break;

	case OP_PUNCH_HOLE:
		TRIM_OFF_LEN(offset, size, file_size);
		do_punch_hole(offset, size);
		break;
	case OP_ZERO_RANGE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		do_zero_range(offset, size, keep_size);
		break;
	case OP_COLLAPSE_RANGE:
		TRIM_OFF_LEN(offset, size, file_size - 1);
		offset = rounddown_64(offset, block_size);
		size = rounddown_64(size, block_size);
		if (size == 0) {
			log4(OP_COLLAPSE_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		do_collapse_range(offset, size);
		break;
	case OP_INSERT_RANGE:
		TRIM_OFF(offset, file_size);
		TRIM_LEN(file_size, size, maxfilelen);
		offset = rounddown_64(offset, block_size);
		size = rounddown_64(size, block_size);
		if (size == 0) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		if (file_size + size > maxfilelen) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}

		do_insert_range(offset, size);
		break;
	case OP_EXCHANGE_RANGE:
		if (size == 0) {
			log5(OP_EXCHANGE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		if (offset2 + size > maxfilelen) {
			log5(OP_EXCHANGE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}

		do_exchange_range(offset, size, offset2);
		break;
	case OP_CLONE_RANGE:
		if (size == 0) {
			log5(OP_CLONE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		if (offset2 + size > maxfilelen) {
			log5(OP_CLONE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}

		do_clone_range(offset, size, offset2);
		break;
	case OP_DEDUPE_RANGE:
		if (size == 0) {
			log5(OP_DEDUPE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		if (offset2 + size > maxfilelen) {
			log5(OP_DEDUPE_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}

		do_dedupe_range(offset, size, offset2);
		break;
	case OP_COPY_RANGE:
		if (size == 0) {
			log5(OP_COPY_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}
		if (offset2 + size > maxfilelen) {
			log5(OP_COPY_RANGE, offset, size, offset2, FL_SKIPPED);
			goto out;
		}

		do_copy_range(offset, size, offset2);
		break;
	case OP_FSYNC:
		dofsync();
		break;
	default:
		prterr("test: unknown operation");
		report_failure(42);
		break;
	}

	if (check_file && testcalls > simulatedopcount)
		check_contents();

out:
	if (closeopen)
		docloseopen();
	if (sizechecks && testcalls > simulatedopcount)
		check_size();
	return 1;
}


void
usage(void)
{
	fprintf(stdout, "usage: %s",
		"fsx [-adfhknqxyzBEFHIJKLORWXZ0]\n\
	   [-b opnum] [-c Prob] [-g filldata] [-i logdev] [-j logid]\n\
	   [-l flen] [-m start:end] [-o oplen] [-p progressinterval]\n\
	   [-r readbdy] [-s style] [-t truncbdy] [-w writebdy]\n\
	   [-A|-U] [-D startingop] [-N numops] [-P dirpath] [-S seed]\n\
	   [--replay-ops=opsfile] [--record-ops[=opsfile]] [--duration=seconds]\n\
	   ... fname\n\
	-a: disable atomic writes\n\
	-b opnum: beginning operation number (default 1)\n\
	-c P: 1 in P chance of file close+open at each op (default infinity)\n\
	-d: debug output for all operations\n\
	-e: pollute post-eof on size changes (default 0)\n\
	-f: flush and invalidate cache after I/O\n\
	-g X: write character X instead of random generated data\n"
#ifdef MADV_COLLAPSE
"	-h hugepages: use buffers backed by hugepages for reads/writes\n"
#endif
"	-i logdev: do integrity testing, logdev is the dm log writes device\n\
	-j logid: prefix debug log messsages with this id\n\
	-k: do not truncate existing file and use its size as upper bound on file size\n\
	-l flen: the upper bound on file size (default 262144)\n\
	-m startop:endop: monitor (print debug output) specified byte range (default 0:infinity)\n\
	-n: no verifications of file size\n\
	-o oplen: the upper bound on operation size (default 65536)\n\
	-p progressinterval: debug output at specified operation interval\n\
	-q: quieter operation\n\
	-r readbdy: 4096 would make reads page aligned (default 1)\n\
	-s style: 1 gives smaller truncates (default 0)\n\
	-t truncbdy: 4096 would make truncates page aligned (default 1)\n"
#ifdef FALLOC_FL_UNSHARE_RANGE
"	-u Do not use unshare range\n"
#endif
"	-w writebdy: 4096 would make writes page aligned (default 1)\n\
	-x: preallocate file space before starting, XFS only\n\
	-y: synchronize changes to a file\n"

#ifdef AIO
"	-A: Use the AIO system calls, -A excludes -U\n"
#endif
#ifdef URING
"	-U: Use the IO_URING system calls, -U excludes -A\n"
#endif
"	-D startingop: debug output starting at specified operation\n"
#ifdef HAVE_LINUX_FALLOC_H
"	-F: Do not use fallocate (preallocation) calls\n"
#endif
#ifdef FALLOC_FL_PUNCH_HOLE
"	-H: Do not use punch hole calls\n"
#endif
#ifdef FALLOC_FL_ZERO_RANGE
"	-z: Do not use zero range calls\n"
#endif
#ifdef FALLOC_FL_COLLAPSE_RANGE
"	-C: Do not use collapse range calls\n"
#endif
#ifdef FALLOC_FL_INSERT_RANGE
"	-I: Do not use insert range calls\n"
#endif
#ifdef FICLONERANGE
"	-J: Do not use clone range calls\n"
#endif
#ifdef FIDEDUPERANGE
"	-B: Do not use dedupe range calls\n"
#endif
#ifdef HAVE_COPY_FILE_RANGE
"	-E: Do not use copy range calls\n"
#endif
#ifdef XFS_IOC_EXCHANGE_RANGE
"	-0: Do not use exchange range calls\n"
#endif
"	-K: Do not use keep size\n\
	-T: Do not use dontcache IO\n\
	-L: fsxLite - no file creations & no file size changes\n\
	-N numops: total # operations to do (default infinity)\n\
	-O: use oplen (see -o flag) for every op (default random)\n\
	-P dirpath: save .fsxlog .fsxops and .fsxgood files in dirpath (default ./)\n\
	-R: read() system calls only (mapped reads disabled)\n\
	-S seed: for random # generator (default 1) 0 gets timestamp\n\
	-W: mapped write operations DISabled\n\
	-X: Read file and compare to good buffer after every operation\n\
	-Z: O_DIRECT (use -R, -W, -r and -w too, excludes dontcache IO)\n\
	--replay-ops=opsfile: replay ops from recorded .fsxops file\n\
	--record-ops[=opsfile]: dump ops file also on success. optionally specify ops file name\n\
	--duration=seconds: ignore any -N setting and run for this many seconds\n\
	fname: this filename is REQUIRED (no default)\n");
	exit(90);
}


long long
getnum(char *s, char **e)
{
	long long ret;

	*e = (char *) 0;
	ret = strtoll(s, e, 0);
	if (*e)
		switch (**e) {
		case 'b':
		case 'B':
			ret *= 512;
			*e = *e + 1;
			break;
		case 'k':
		case 'K':
			ret *= 1024;
			*e = *e + 1;
			break;
		case 'm':
		case 'M':
			ret *= 1024*1024;
			*e = *e + 1;
			break;
		case 'w':
		case 'W':
			ret *= 4;
			*e = *e + 1;
			break;
		}
	return (ret);
}

#ifdef AIO

#define QSZ     1024
io_context_t	io_ctx;
struct iocb 	iocb;

int
aio_setup()
{
	int ret;
	ret = io_queue_init(QSZ, &io_ctx);
	if (ret != 0) {
		fprintf(stderr, "aio_setup: io_queue_init failed: %s\n",
                        strerror(ret));
                return(-1);
        }
        return(0);
}

int
aio_rw(int rw, int fd, char *buf, unsigned len, unsigned offset)
{
	struct io_event event;
	static struct timespec ts;
	struct iocb *iocbs[] = { &iocb };
	int ret;
	long res;

	if (rw == READ) {
		io_prep_pread(&iocb, fd, buf, len, offset);
	} else {
		io_prep_pwrite(&iocb, fd, buf, len, offset);
	}

	ts.tv_sec = 30;
	ts.tv_nsec = 0;
	ret = io_submit(io_ctx, 1, iocbs);
	if (ret != 1) {
		fprintf(stderr, "errcode=%d\n", ret);
		fprintf(stderr, "aio_rw: io_submit failed: %s\n",
				strerror(ret));
		goto out_error;
	}

	ret = io_getevents(io_ctx, 1, 1, &event, &ts);
	if (ret != 1) {
		if (ret == 0)
			fprintf(stderr, "aio_rw: no events available\n");
		else {
			fprintf(stderr, "errcode=%d\n", -ret);
			fprintf(stderr, "aio_rw: io_getevents failed: %s\n",
				 	strerror(-ret));
		}
		goto out_error;
	}
	if (len != event.res) {
		/*
		 * The b0rked libaio defines event.res as unsigned.
		 * However the kernel strucuture has it signed,
		 * and it's used to pass negated error value.
		 * Till the library is fixed use the temp var.
		 */
		res = (long)event.res;
		if (res >= 0)
			fprintf(stderr, "bad io length: %lu instead of %u\n",
					res, len);
		else {
			fprintf(stderr, "errcode=%ld\n", -res);
			fprintf(stderr, "aio_rw: async io failed: %s\n",
					strerror(-res));
			ret = res;
			goto out_error;
		}

	}
	return event.res;

out_error:
	/*
	 * The caller expects error return in traditional libc
	 * convention, i.e. -1 and the errno set to error.
	 */
	errno = -ret;
	return -1;
}
#else
int
aio_rw(int rw, int fd, char *buf, unsigned len, unsigned offset)
{
	fprintf(stderr, "io_rw: need AIO support!\n");
	exit(111);
}
#endif

#ifdef URING

struct io_uring ring;
#define URING_ENTRIES	1024

int
uring_setup()
{
	int ret;

	ret = io_uring_queue_init(URING_ENTRIES, &ring, 0);
	if (ret != 0) {
		fprintf(stderr, "uring_setup: io_uring_queue_init failed: %s\n",
				strerror(ret));
		return -1;
	}
	return 0;
}

int
uring_rw(int rw, int fd, char *buf, unsigned len, unsigned offset, int flags)
{
	struct io_uring_sqe     *sqe;
	struct io_uring_cqe     *cqe;
	struct iovec            iovec;
	int ret;
	int res = 0;
	char *p = buf;
	unsigned l = len;
	unsigned o = offset;

	/*
	 * Due to io_uring tries non-blocking IOs (especially read), that
	 * always cause 'normal' short reading. To avoid this short read
	 * fail, try to loop read/write (escpecilly read) data.
	 */
	while (l > 0) {
		sqe = io_uring_get_sqe(&ring);
		if (!sqe) {
			fprintf(stderr, "uring_rw: io_uring_get_sqe failed: %s\n",
					strerror(errno));
			return -1;
		}

		iovec.iov_base = p;
		iovec.iov_len = l;
		if (rw == READ) {
			io_uring_prep_readv(sqe, fd, &iovec, 1, o);
		} else {
			io_uring_prep_writev(sqe, fd, &iovec, 1, o);
		}
		sqe->rw_flags = flags;

		ret = io_uring_submit_and_wait(&ring, 1);
		if (ret != 1) {
			fprintf(stderr, "errcode=%d\n", -ret);
			fprintf(stderr, "uring %s: io_uring_submit failed: %s\n",
					rw == READ ? "read":"write", strerror(-ret));
			goto uring_error;
		}

		ret = io_uring_wait_cqe(&ring, &cqe);
		if (ret != 0) {
			fprintf(stderr, "errcode=%d\n", -ret);
			fprintf(stderr, "uring %s: io_uring_wait_cqe failed: %s\n",
					rw == READ ? "read":"write", strerror(-ret));
			goto uring_error;
		}

		ret = cqe->res;
		io_uring_cqe_seen(&ring, cqe);

		if (ret > 0) {
			o += ret;
			l -= ret;
			p += ret;
			res += ret;
		} else if (ret < 0) {
			fprintf(stderr, "errcode=%d\n", -ret);
			fprintf(stderr, "uring %s: io_uring failed: %s\n",
					rw == READ ? "read":"write", strerror(-ret));
			goto uring_error;
		} else {
			fprintf(stderr, "uring %s bad io length: %d instead of %u\n",
					rw == READ ? "read":"write", res, len);
			break;
		}
	}
	return res;

 uring_error:
	/*
	 * The caller expects error return in traditional libc
	 * convention, i.e. -1 and the errno set to error.
	 */
	errno = -ret;
	return -1;
}
#else
int
uring_rw(int rw, int fd, char *buf, unsigned len, unsigned offset, int flags)
{
	fprintf(stderr, "io_rw: need IO_URING support!\n");
	exit(111);
}
#endif

int
fsx_rw(int rw, int fd, char *buf, unsigned len, unsigned offset, int flags)
{
	int ret;

	if (aio) {
		ret = aio_rw(rw, fd, buf, len, offset);
	} else if (uring) {
		ret = uring_rw(rw, fd, buf, len, offset, flags);
	} else {
		struct iovec iov = { .iov_base = buf, .iov_len = len };

		if (rw == READ)
			ret = preadv2(fd, &iov, 1, offset, flags);
		else
			ret = pwritev2(fd, &iov, 1, offset, flags);
	}
	return ret;
}

#define test_fallocate(mode) __test_fallocate(mode, #mode)

int
__test_fallocate(int mode, const char *mode_str)
{
#ifdef HAVE_LINUX_FALLOC_H
	int ret = 0;
	if (!lite) {
		if (fallocate(fd, mode, file_size, 1) &&
		    (errno == ENOSYS || errno == EOPNOTSUPP)) {
			if(!quiet)
				fprintf(stderr,
					"main: filesystem does not support "
					"fallocate mode %s, disabling!\n",
					mode_str);
		} else {
			ret = 1;
			if (ftruncate(fd, file_size)) {
				warn("main: ftruncate");
				exit(132);
			}
		}
	}
	return ret;
#endif
}

/*
 * Reclaim may break up hugepages, so do a best-effort collapse every once in
 * a while.
 */
static void
collapse_hugepages(void)
{
#ifdef MADV_COLLAPSE
	int ret;

	/* re-collapse every 16k fsxops after we start */
	if (!numops || (numops & ((1U << 14) - 1)))
		return;

	ret = madvise(hugepages_info.orig_good_buf,
		      hugepages_info.good_buf_size, MADV_COLLAPSE);
	if (ret)
		prt("collapsing hugepages for good_buf failed (numops=%llu): %s\n",
		     numops, strerror(errno));
	ret = madvise(hugepages_info.orig_temp_buf,
		      hugepages_info.temp_buf_size, MADV_COLLAPSE);
	if (ret)
		prt("collapsing hugepages for temp_buf failed (numops=%llu): %s\n",
		     numops, strerror(errno));
#endif
}

bool
keep_running(void)
{
	int ret;

	if (hugepages)
	        collapse_hugepages();

	if (deadline.tv_nsec) {
		struct timespec now;

		ret = clock_gettime(CLOCK_MONOTONIC, &now);
		if (ret) {
			perror("CLOCK_MONOTONIC");
			return false;
		}

		return now.tv_sec <= deadline.tv_sec;
	}

	if (numops == -1)
		return true;

	return numops-- != 0;
}

static long
get_hugepage_size(void)
{
	const char str[] = "Hugepagesize:";
	size_t str_len =  sizeof(str) - 1;
	unsigned int hugepage_size = 0;
	char buffer[64];
	FILE *file;

	file = fopen("/proc/meminfo", "r");
	if (!file) {
		prterr("get_hugepage_size: fopen /proc/meminfo");
		return -1;
	}
	while (fgets(buffer, sizeof(buffer), file)) {
		if (strncmp(buffer, str, str_len) == 0) {
			sscanf(buffer + str_len, "%u", &hugepage_size);
			break;
		}
	}
	fclose(file);
	if (!hugepage_size) {
		prterr("get_hugepage_size: failed to find "
			"hugepage size in /proc/meminfo\n");
		return -1;
	}

	/* convert from KiB to bytes */
	return hugepage_size << 10;
}

static void *
init_hugepages_buf(unsigned len, int hugepage_size, int alignment, long *buf_size)
{
	void *buf = NULL;
#ifdef MADV_COLLAPSE
	int ret;
	long size = roundup(len, hugepage_size) + alignment;

	ret = posix_memalign(&buf, hugepage_size, size);
	if (ret) {
		/* common/rc greps this error message */
		prterr("posix_memalign for hugebuf");
		return NULL;
	}
	memset(buf, '\0', size);
	ret = madvise(buf, size, MADV_COLLAPSE);
	if (ret) {
		/* common/rc greps this error message */
		prterr("madvise collapse for hugebuf");
		free(buf);
		return NULL;
	}

	*buf_size = size;
#endif
	return buf;
}

static void
init_buffers(void)
{
	int i;

	original_buf = (char *) malloc(maxfilelen);
	for (i = 0; i < maxfilelen; i++)
		original_buf[i] = random() % 256;
	if (hugepages) {
		long hugepage_size = get_hugepage_size();
		if (hugepage_size == -1) {
			prterr("get_hugepage_size()");
			exit(102);
		}
		good_buf = init_hugepages_buf(maxfilelen, hugepage_size, writebdy,
					      &hugepages_info.good_buf_size);
		if (!good_buf) {
			prterr("init_hugepages_buf failed for good_buf");
			exit(103);
		}
		hugepages_info.orig_good_buf = good_buf;

		temp_buf = init_hugepages_buf(maxoplen, hugepage_size, readbdy,
					      &hugepages_info.temp_buf_size);
		if (!temp_buf) {
			prterr("init_hugepages_buf failed for temp_buf");
			exit(103);
		}
		hugepages_info.orig_temp_buf = temp_buf;
	} else {
		unsigned long good_buf_len = maxfilelen + writebdy;
		unsigned long temp_buf_len = maxoplen + readbdy;

		good_buf = calloc(1, good_buf_len);
		temp_buf = calloc(1, temp_buf_len);
	}
	good_buf = round_ptr_up(good_buf, writebdy, 0);
	temp_buf = round_ptr_up(temp_buf, readbdy, 0);
}

static struct option longopts[] = {
	{"replay-ops", required_argument, 0, 256},
	{"record-ops", optional_argument, 0, 255},
	{"duration", optional_argument, 0, 254},
	{ }
};

int
main(int argc, char **argv)
{
	int	i, style, ch;
	char	*endp, *tmp;
	char logfile[PATH_MAX];
	struct stat statbuf;
	int o_flags = O_RDWR|O_CREAT|O_TRUNC;
	long long duration;

	logfile[0] = 0;
	dname[0] = 0;

	page_size = getpagesize();
	page_mask = page_size - 1;
	mmap_mask = page_mask;

	setvbuf(stdout, (char *)0, _IOLBF, 0); /* line buffered stdout */

	while ((ch = getopt_long(argc, argv,
				 "0ab:c:de:fg:hi:j:kl:m:no:p:qr:s:t:uw:xyABD:EFJKHzCILN:OP:RS:UWXZ",
				 longopts, NULL)) != EOF)
		switch (ch) {
		case 'a':
			prt("main(): Atomic writes disabled\n");
			do_atomic_writes = 0;
			break;
		case 'b':
			simulatedopcount = getnum(optarg, &endp);
			if (!quiet)
				prt("Will begin at operation %lld\n",
						simulatedopcount);
			if (simulatedopcount == 0)
				usage();
			simulatedopcount -= 1;
			break;
		case 'c':
			closeprob = getnum(optarg, &endp);
			if (!quiet)
				prt("Chance of close/open is 1 in %d\n", closeprob);
			if (closeprob <= 0)
				usage();
			break;
		case 'd':
			debug = 1;
			break;
		case 'e':
			pollute_eof = getnum(optarg, &endp);
			if (pollute_eof < 0 || pollute_eof > 1)
				usage();
			break;
		case 'f':
			flush = 1;
			break;
		case 'g':
			filldata = *optarg;
			break;
		case 'h':
#ifndef MADV_COLLAPSE
			fprintf(stderr, "MADV_COLLAPSE not supported. "
				"Can't support -h\n");
			exit(86);
#endif
			hugepages = 1;
			break;
		case 'i':
			integrity = 1;
			logdev = strdup(optarg);
			if (!logdev) {
				prterr("strdup");
				exit(101);
			}
			break;
		case 'j':
			logid = strdup(optarg);
			if (!logid) {
				prterr("strdup");
				exit(101);
			}
			break;
		case 'k':
			o_flags &= ~O_TRUNC;
			break;
		case 'l':
			maxfilelen = getnum(optarg, &endp);
			if (maxfilelen <= 0)
				usage();
			break;
		case 'm':
			monitorstart = getnum(optarg, &endp);
			if (monitorstart < 0)
				usage();
			if (!endp || *endp++ != ':')
				usage();
			monitorend = getnum(endp, &endp);
			if (monitorend < 0)
				usage();
			if (monitorend == 0)
				monitorend = -1; /* aka infinity */
			debug = 1;
		case 'n':
			sizechecks = 0;
			break;
		case 'o':
			maxoplen = getnum(optarg, &endp);
			if (maxoplen <= 0)
				usage();
			break;
		case 'p':
			progressinterval = getnum(optarg, &endp);
			if (progressinterval == 0)
				usage();
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			readbdy = getnum(optarg, &endp);
			if (readbdy <= 0)
				usage();
			break;
		case 's':
			style = getnum(optarg, &endp);
			if (style < 0 || style > 1)
				usage();
			break;
		case 't':
			truncbdy = getnum(optarg, &endp);
			if (truncbdy <= 0)
				usage();
			break;
		case 'u':
			unshare_range_calls = 0;
			break;
		case 'w':
			writebdy = getnum(optarg, &endp);
			if (writebdy <= 0)
				usage();
			break;
		case 'x':
			prealloc = 1;
			break;
		case 'y':
			do_fsync = 1;
			break;
		case 'A':
			aio = 1;
			break;
		case 'U':
			uring = 1;
			break;
		case 'D':
			debugstart = getnum(optarg, &endp);
			if (debugstart < 1)
				usage();
			break;
		case 'F':
			fallocate_calls = 0;
			break;
		case 'K':
			keep_size_calls = 0;
			break;
		case 'H':
			punch_hole_calls = 0;
			break;
		case 'z':
			zero_range_calls = 0;
			break;
		case 'C':
			collapse_range_calls = 0;
			break;
		case 'I':
			insert_range_calls = 0;
			break;
		case '0':
			exchange_range_calls = 0;
			break;
		case 'J':
			clone_range_calls = 0;
			break;
		case 'B':
			dedupe_range_calls = 0;
			break;
		case 'E':
			copy_range_calls = 0;
			break;
		case 'L':
		        lite = 1;
			o_flags &= ~(O_CREAT|O_TRUNC);
			break;
		case 'N':
			numops = getnum(optarg, &endp);
			if (numops < 0)
				usage();
			break;
		case 'O':
			randomoplen = 0;
			break;
		case 'P':
			snprintf(dname, sizeof(dname), "%s/", optarg);
			dirpath = strlen(dname);
			break;
                case 'R':
                        mapped_reads = 0;
                        break;
		case 'S':
                        seed = getnum(optarg, &endp);
			if (seed == 0) {
				seed = time(0) % 10000;
				seed += (int)getpid();
			}
			if (seed < 0)
				usage();
			break;
		case 'T':
			dontcache_io = 0;
			break;
		case 'W':
		        mapped_writes = 0;
			if (!quiet)
				prt("mapped writes DISABLED\n");
			break;
		case 'X':
			check_file = 1;
			break;
		case 'Z':
			o_direct = O_DIRECT;
			o_flags |= O_DIRECT;
			dontcache_io = 0;
			break;
		case 254:  /* --duration */
			if (!optarg) {
				fprintf(stderr, "Specify time with --duration=\n");
				exit(87);
			}
			duration = strtoll(optarg, NULL, 0);
			if (duration < 1) {
				fprintf(stderr, "%lld: invalid duration\n", duration);
				exit(88);
			}

			i = clock_gettime(CLOCK_MONOTONIC, &deadline);
			if (i) {
				perror("CLOCK_MONOTONIC");
				exit(89);
			}

			deadline.tv_sec += duration;
			deadline.tv_nsec = 1;
			break;
		case 255:  /* --record-ops */
			if (optarg)
				snprintf(opsfile, sizeof(opsfile), "%s", optarg);
			recordops = opsfile;
			break;
		case 256:  /* --replay-ops */
			replayops = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	if (aio && uring) {
		fprintf(stderr, "-A and -U shouldn't be used together\n");
		usage();
	}

	if (integrity && !dirpath) {
		fprintf(stderr, "option -i <logdev> requires -P <dirpath>\n");
		usage();
	}

	fname = argv[0];
	tmp = strdup(fname);
	if (!tmp) {
		prterr("strdup");
		exit(101);
	}
	bname = basename(tmp);

	signal(SIGHUP,	cleanup);
	signal(SIGINT,	cleanup);
	signal(SIGPIPE,	cleanup);
	signal(SIGALRM,	cleanup);
	signal(SIGTERM,	cleanup);
	signal(SIGXCPU,	cleanup);
	signal(SIGXFSZ,	cleanup);
	signal(SIGVTALRM,	cleanup);
	signal(SIGUSR1,	cleanup);
	signal(SIGUSR2,	cleanup);

	if (!quiet && seed)
		prt("Seed set to %d\n", seed);
	srandom(seed);
	fd = open(fname, o_flags, 0666);
	if (fd < 0) {
		prterr(fname);
		exit(91);
	}
	if (fstat(fd, &statbuf)) {
		prterr("check_size: fstat");
		exit(91);
	}
	block_size = statbuf.st_blksize;
#ifdef XFS
	if (prealloc) {
		xfs_flock64_t	resv = { 0 };
#ifdef HAVE_XFS_PLATFORM_DEFS_H
		if (!platform_test_xfs_fd(fd)) {
			prterr(fname);
			fprintf(stderr, "main: cannot prealloc, non XFS\n");
			exit(96);
		}
#endif
		resv.l_len = maxfilelen;
		if ((xfsctl(fname, fd, XFS_IOC_RESVSP, &resv)) < 0) {
			prterr(fname);
			exit(97);
		}
	}
#endif

	if (dirpath) {
		snprintf(goodfile, sizeof(goodfile), "%s%s.fsxgood", dname, bname);
		snprintf(logfile, sizeof(logfile), "%s%s.fsxlog", dname, bname);
		if (!*opsfile)
			snprintf(opsfile, sizeof(opsfile), "%s%s.fsxops", dname, bname);
	} else {
		snprintf(goodfile, sizeof(goodfile), "%s.fsxgood", fname);
		snprintf(logfile, sizeof(logfile), "%s.fsxlog", fname);
		if (!*opsfile)
			snprintf(opsfile, sizeof(opsfile), "%s.fsxops", fname);
	}
	fsxgoodfd = open(goodfile, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fsxgoodfd < 0) {
		prterr(goodfile);
		exit(92);
	}
	fsxlogf = fopen(logfile, "w");
	if (fsxlogf == NULL) {
		prterr(logfile);
		exit(93);
	}
	unlink(opsfile);

	if (replayops) {
		replayopsf = fopen(replayops, "r");
		if (!replayopsf) {
			prterr(replayops);
			exit(93);
		}
	}

#ifdef AIO
	if (aio) 
		aio_setup();
#endif
#ifdef URING
	if (uring)
		uring_setup();
#endif

	if (!(o_flags & O_TRUNC)) {
		off_t ret;
		file_size = maxfilelen = biggest = lseek(fd, (off_t)0, SEEK_END);
		if (file_size == (off_t)-1) {
			prterr(fname);
			warn("main: lseek eof");
			exit(94);
		}
		ret = lseek(fd, (off_t)0, SEEK_SET);
		if (ret == (off_t)-1) {
			prterr(fname);
			warn("main: lseek 0");
			exit(95);
		}
	}
	init_buffers();
	if (lite) {	/* zero entire existing file */
		ssize_t written;

		written = write(fd, good_buf, (size_t)maxfilelen);
		if (written != maxfilelen) {
			if (written == -1) {
				prterr(fname);
				warn("main: error on write");
			} else
				warn("main: short write, 0x%x bytes instead "
					"of 0x%lx\n",
					(unsigned)written,
					maxfilelen);
			exit(98);
		}
	} else {
		ssize_t ret, len = file_size;
		off_t off = 0;

		while (len > 0) {
			ret = read(fd, good_buf + off, len);
			if (ret == -1) {
				prterr(fname);
				warn("main: error on read");
				exit(98);
			}
			len -= ret;
			off += ret;
		}

		check_trunc_hack();
	}

	if (fallocate_calls)
		fallocate_calls = test_fallocate(0);
	if (keep_size_calls)
		keep_size_calls = test_fallocate(FALLOC_FL_KEEP_SIZE);
	if (unshare_range_calls)
		unshare_range_calls = test_fallocate(FALLOC_FL_UNSHARE_RANGE);
	if (punch_hole_calls)
		punch_hole_calls = test_fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE);
	if (zero_range_calls)
		zero_range_calls = test_fallocate(FALLOC_FL_ZERO_RANGE);
	if (collapse_range_calls)
		collapse_range_calls = test_fallocate(FALLOC_FL_COLLAPSE_RANGE);
	if (insert_range_calls)
		insert_range_calls = test_fallocate(FALLOC_FL_INSERT_RANGE);
	if (clone_range_calls)
		clone_range_calls = test_clone_range();
	if (dedupe_range_calls)
		dedupe_range_calls = test_dedupe_range();
	if (copy_range_calls)
		copy_range_calls = test_copy_range();
	if (exchange_range_calls)
		exchange_range_calls = test_exchange_range();
	if (dontcache_io)
		dontcache_io = test_dontcache_io();
	if (do_atomic_writes)
		do_atomic_writes = test_atomic_writes();

	while (keep_running())
		if (!test())
			break;

	free(tmp);
	if (close(fd)) {
		prterr("close");
		report_failure(99);
	}
	prt("All %lld operations completed A-OK!\n", testcalls);
	if (recordops)
		logdump();

	fclose(fsxlogf);
	exit(0);
	return 0;
}
