/* E4COMPACT
 *
 * Compact list of files sequentially
 *
 * Usage example:
 * find /etc -type f > etc_list
 * fallocate -l100M /etc/.tmp_donor_file
 * cat etc_list | ./e4defrag /etc/.tmp_donor_file
 * unlink /etc/.tmp_donor_file
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <linux/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#ifndef EXT4_IOC_MOVE_EXT
struct move_extent {
	__s32 reserved;	/* original file descriptor */
	__u32 donor_fd;	/* donor file descriptor */
	__u64 orig_start;	/* logical start offset in block for orig */
	__u64 donor_start;	/* logical start offset in block for donor */
	__u64 len;	/* block length to be moved */
	__u64 moved_len;	/* moved block length */
};

#define EXT4_IOC_MOVE_EXT      _IOWR('f', 15, struct move_extent)
#endif

struct donor_info
{
	int fd;
	__u64 offset;
	__u64 length;
};

static int ignore_error = 0;
static int verbose = 0;
static unsigned blk_per_pg;
static unsigned blk_sz;


static int do_defrag_one(int fd, char *name,__u64 start, __u64 len, struct donor_info *donor)
{
	int ret, retry;
	struct move_extent mv_ioc;
	__u64 moved = 0;
	int i = 0;

	assert(donor->length >= len);
	/* EXT4_IOC_MOVE_EXT requires both files has same offset inside page */
	donor->offset += (blk_per_pg - (donor->offset  & (blk_per_pg -1)) +
			 (start  & (blk_per_pg -1))) & (blk_per_pg -1);

	mv_ioc.donor_fd = donor->fd;
	mv_ioc.orig_start = start;
	mv_ioc.donor_start = donor->offset;
	mv_ioc.len = len;

	if (verbose)
		printf("%s %s start:%lld len:%lld donor [%lld, %lld]\n",  __func__,
		       name, (unsigned long long) start,
		       (unsigned long long) len,
		       (unsigned long long)donor->offset,
		       (unsigned long long)donor->length);
	retry= 3;
	do {
		i++;
		errno = 0;
		mv_ioc.moved_len = 0;
		ret = ioctl(fd, EXT4_IOC_MOVE_EXT, &mv_ioc);
		if (verbose)
			printf("process %s  it:%d start:%lld len:%lld donor:%lld,"
			       "moved:%lld ret:%d errno:%d\n",
			       name, i,
			       (unsigned long long) mv_ioc.orig_start,
			       (unsigned long long) mv_ioc.len,
			       (unsigned long long)mv_ioc.donor_start,
			       (unsigned long long)mv_ioc.moved_len,
		       ret, errno);
		if (ret < 0) {
			if (verbose)
				printf("%s EXT4_IOC_MOVE_EXT failed err:%d\n",
				       __func__, errno);
			if (errno != EBUSY || !retry--)
				break;
		} else {
			retry = 3;
			/* Nothing to swap */
			if (mv_ioc.moved_len == 0)
				break;
		}
		assert(mv_ioc.len >= mv_ioc.moved_len);
		mv_ioc.len -= mv_ioc.moved_len;
		mv_ioc.orig_start += mv_ioc.moved_len;
		mv_ioc.donor_start += mv_ioc.moved_len;
		moved += mv_ioc.moved_len;
	} while (mv_ioc.len);

	if (ret && ignore_error &&
	    (errno == EBUSY || errno == ENODATA || errno == EOPNOTSUPP))
		ret = 0;
	donor->length -= moved;
	donor->offset += moved;
	return ret;
}

void usage()
{
	printf("Usage: -f donor_file [-o donor_offset] [-v] [-i]\n"
	       "\t\t -v: verbose\n"
	       "\t\t -i: ignore errors\n");
}

int main(int argc, char **argv)
{
	int fd, ret = 0;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	struct donor_info donor;
	struct stat st;
	extern char *optarg;
	extern int optind;
	int c;
	char * donor_name = NULL;

	donor.offset = 0;
	while ((c = getopt(argc, argv, "f:o:iv")) != -1) {
		switch (c) {
		case 'o':
			donor.offset = atol(optarg);
			break;
		case 'i':
			ignore_error = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'f':
			donor_name = (optarg);
			break;

		default:
			usage();
			exit(1);
		}
	}
	if (!donor_name) {
		usage();
		exit(1);
	}
	donor.fd = open(donor_name, O_RDWR);
	if (donor.fd < 0) {
		perror("can not open donor file");
		exit(1);
	}
	if (fstat(donor.fd, &st)) {
		perror("can not stat donor fd");
		exit(1);
	}
	donor.length = st.st_size / st.st_blksize;
	if (donor.offset)
		donor.offset /= st.st_blksize;
	blk_sz = st.st_blksize;
	blk_per_pg = sysconf(_SC_PAGESIZE) / blk_sz;

	if (verbose)
		printf("Init donor %s off:%lld len:%lld bsz:%lu\n",
		       donor_name, donor.offset, donor.length, st.st_blksize);

	while ((read = getline(&line, &len, stdin)) != -1) {

		if (line[read -1] == '\n')
			line[read -1] = 0;

		fd = open(line, O_RDWR);
		if (fd < 0) {
			if (verbose)
				printf("Can not open %s errno:%d\n", line, errno);
			if (ignore_error)
				continue;
			else
				break;
		}
		if(fstat(fd, &st)) {
			if (verbose)
				perror("Can not stat ");
			continue;
			if (ignore_error)
				continue;
			else
				break;

		}
		if (st.st_size && st.st_blocks) {
			ret = do_defrag_one(fd, line, 0,
					    (st.st_size  + blk_sz-1)/ blk_sz,
					    &donor);
			if (ret && !ignore_error)
				break;
		}
	}
	free(line);
	return ret;
}
