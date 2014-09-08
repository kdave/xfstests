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
#include <linux/fs.h>
#include <linux/fiemap.h>
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
#define EXTENT_MAX_COUNT        512

struct donor_info
{
	int fd;
	__u64 offset;
	__u64 length;
};

static int ignore_error = 0;
static int verbose = 0;
static int do_sparse = 0;
static unsigned blk_per_pg;
static unsigned blk_sz;

static int do_defrag_range(int fd, char *name,__u64 start, __u64 len,
			   struct donor_info *donor)
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

static int do_defrag_sparse(int fd, char *name,__u64 start, __u64 len,
			    struct donor_info *donor)
{
	int i, ret = 0;
	struct fiemap	*fiemap_buf = NULL;
	struct fiemap_extent	*ext_buf = NULL;

	fiemap_buf = malloc(EXTENT_MAX_COUNT * sizeof(struct fiemap_extent)
			    + sizeof(struct fiemap));
	if (fiemap_buf == NULL) {
		fprintf(stderr, "%s Can not allocate memory\n", __func__);
		return -1;
	}
	ext_buf = fiemap_buf->fm_extents;
	memset(fiemap_buf, 0, sizeof(struct fiemap));
	fiemap_buf->fm_flags |= FIEMAP_FLAG_SYNC;
	fiemap_buf->fm_extent_count = EXTENT_MAX_COUNT;

	do {
		__u64 next;

		fiemap_buf->fm_start = start * blk_sz;
		fiemap_buf->fm_length = len * blk_sz;
		ret = ioctl(fd, FS_IOC_FIEMAP, fiemap_buf);
		if (ret < 0 || fiemap_buf->fm_mapped_extents == 0) {
			fprintf(stderr, "%s Can get extent info for %s ret:%d mapped:%d",
				__func__, name, ret, fiemap_buf->fm_mapped_extents);
			goto out;
		}
		for (i = 0; i < fiemap_buf->fm_mapped_extents; i++) {
			ret = do_defrag_range(fd, name,
					      ext_buf[i].fe_logical / blk_sz,
					      ext_buf[i].fe_length / blk_sz,
					      donor);
			if (ret)
				goto out;
		}
		next = (ext_buf[fiemap_buf->fm_mapped_extents -1].fe_logical +
			   ext_buf[fiemap_buf->fm_mapped_extents -1].fe_length) /
			blk_sz;
		if (next <  start + len) {
			len -= next  - start;
			start = next;
		} else
			break;

	} while (fiemap_buf->fm_mapped_extents == EXTENT_MAX_COUNT &&
		 !(ext_buf[EXTENT_MAX_COUNT-1].fe_flags & FIEMAP_EXTENT_LAST));
out:
	free(fiemap_buf);
	return ret;
}

void usage()
{
	printf("Usage: -f donor_file [-o donor_offset] [-v] [-i]\n"
	       "\t\t -v: verbose\n"
	       "\t\t -s: enable sparse file optimization\n"
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
	__u64 eof_blk;

	donor.offset = 0;
	while ((c = getopt(argc, argv, "f:o:isv")) != -1) {
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
		case 's':
			do_sparse = 1;
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
		if (!(st.st_size && st.st_blocks))
			continue;

		eof_blk = (st.st_size + blk_sz-1) / blk_sz;
		if (do_sparse)
			ret = do_defrag_sparse(fd, line, 0, eof_blk, &donor);
		else
			ret = do_defrag_range(fd, line, 0, eof_blk, &donor);
		if (ret && !ignore_error)
			break;

	}
	free(line);
	return ret;
}
