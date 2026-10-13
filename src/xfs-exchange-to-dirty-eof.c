// SPDX-License-Identifier: GPL-2.0
/*
 * Clears XFS_DIFLAG2_REFLINK from a file that still owns a shared block, on a
 * kernel carrying "xfs: fix exchange-range-to-eof file size exchange".
 *
 * Needs reflink=1 and exchange=1, and a readable file called "peer" in the
 * current directory.  Brackets are physical block numbers from FIEMAP.
 * Exits 0 if peer was rewritten, 1 if it survived, 2 on a setup error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <errno.h>

#ifndef FICLONERANGE
#define FICLONERANGE _IOW(0x94, 13, struct file_clone_range)
#endif

#ifndef XFS_EXCHANGE_RANGE_TO_EOF
struct xfs_exchange_range {
	__s32		file1_fd;
	__u32		pad;
	__u64		file1_offset;
	__u64		file2_offset;
	__u64		length;
	__u64		flags;
};
#define XFS_IOC_EXCHANGE_RANGE		_IOW('X', 129, struct xfs_exchange_range)
#define XFS_EXCHANGE_RANGE_TO_EOF	(1ULL << 0)
#endif

static unsigned long blksz;
static int peer, f1;

static unsigned long long phys(int fd, unsigned long long off)
{
	struct { struct fiemap f; struct fiemap_extent e[1]; } q;

	memset(&q, 0, sizeof q);
	q.f.fm_start = off;
	q.f.fm_length = blksz;
	q.f.fm_extent_count = 1;
	if (ioctl(fd, FS_IOC_FIEMAP, &q.f) || q.f.fm_mapped_extents < 1)
		return 0;
	return (q.e[0].fe_physical + (off - q.e[0].fe_logical)) / blksz;
}

static void show(const char *tag)
{
	struct stat st;
	int i;

	fstat(f1, &st);
	printf("%-28s peer [%llu]   file1 isize %5lld  [", tag, phys(peer, 0),
	       (long long)st.st_size);
	for (i = 0; i < 3; i++)
		printf("%llu%s", phys(f1, (unsigned long long)i * blksz),
		       i == 2 ? "]\n" : "] [");
}

/* fsync == 0 leaves every block dirty, so i_disk_size stays 0 */
static int mkfile(const char *name, char fill, int blocks, int do_fsync)
{
	char *buf;
	int fd;

	unlink(name);
	fd = open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return fprintf(stderr, "open %s: %m\n", name), -1;
	buf = malloc((size_t)blocks * blksz);
	memset(buf, fill, (size_t)blocks * blksz);
	if (pwrite(fd, buf, (size_t)blocks * blksz, 0) !=
	    (ssize_t)((size_t)blocks * blksz))
		return fprintf(stderr, "pwrite %s: %m\n", name), -1;
	free(buf);
	if (do_fsync)
		fsync(fd);
	return fd;
}

static int exchange(int fd2, int fd1, unsigned long long off1,
		    unsigned long long off2, unsigned long long len,
		    unsigned long long flags, const char *what)
{
	struct xfs_exchange_range xr;

	memset(&xr, 0, sizeof xr);
	xr.file1_fd = fd1;
	xr.file1_offset = off1;
	xr.file2_offset = off2;
	xr.length = len;
	xr.flags = flags;
	if (ioctl(fd2, XFS_IOC_EXCHANGE_RANGE, &xr)) {
		fprintf(stderr, "%s: %m\n", what);
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct file_clone_range cr;
	char before[65536], after[65536], *buf;
	struct statfs sfs;
	struct stat st;
	int dirty, d2;

	if (argc != 3) {
		fprintf(stderr, "Usage: $0 dir path_to_peer\n");
		return 2;
	}

	if (chdir(argv[1]))
		return perror(argv[1]), 2;

	if (statfs(".", &sfs))
		return fprintf(stderr, "statfs: %m\n"), 2;
	blksz = sfs.f_bsize;
	if (blksz < 512 || blksz > 65536)
		return fprintf(stderr, "unexpected block size %lu\n", blksz), 2;

	peer = open(argv[2], O_RDONLY);
	if (peer < 0)
		return fprintf(stderr, "open peer: %m\n"), 2;
	if (fstat(peer, &st))
		return fprintf(stderr, "fstat peer: %m\n"), 2;
	printf("peer uid %u mode 0%o size %lld, block size %lu\n",
	       st.st_uid, st.st_mode & 07777, (long long)st.st_size, blksz);
	if ((unsigned long)st.st_size < blksz)
		return fprintf(stderr,
			"peer must be at least one block\n"), 2;
	if (pread(peer, before, blksz, 0) != (ssize_t)blksz)
		return fprintf(stderr, "pread peer: %m\n"), 2;
	syncfs(peer);

	/* file1: three blocks, clean, so its own i_disk_size is honest */
	f1 = mkfile("file1", 'A', 3, 1);
	if (f1 < 0)
		return 2;
	/* the lever: one block, entirely dirty, i_disk_size == 0 */
	dirty = mkfile("dirty", 'D', 1, 0);
	if (dirty < 0)
		return 2;
	d2 = mkfile("donor2", 'E', 1, 1);
	if (d2 < 0)
		return 2;
	printf("\n");
	show("start");

	/* share peer's block into file1's middle block */
	memset(&cr, 0, sizeof cr);
	cr.src_fd = peer;
	cr.src_offset = 0;
	cr.src_length = blksz;
	cr.dest_offset = blksz;
	if (ioctl(f1, FICLONERANGE, &cr)) {
		fprintf(stderr, "FICLONERANGE: %m\n");
		if (errno == EOPNOTSUPP)
			fprintf(stderr,
				"this filesystem needs reflink=1, check xfs_info\n");
		return 2;
	}
	show("1 FICLONERANGE");

	/*
	 * ip1 = file1, ip2 = dirty.  off2 == i_size(dirty) == blksz, so
	 *	length = max(3*blksz - 2*blksz, blksz - blksz) = blksz
	 * and dirty's flush range [blksz, 2*blksz) is entirely past its EOF,
	 * so its page cache is never written back and i_disk_size stays 0:
	 *	isize1 = 2*blksz + (0 - blksz) = blksz
	 * file1's ondisk size becomes one block while it still owns the
	 * shared block at offset blksz.
	 */
	if (exchange(dirty, f1, 2 * blksz, blksz, 0,
		     XFS_EXCHANGE_RANGE_TO_EOF, "exchange TO_EOF"))
		return 2;
	show("2 EXCHANGE_RANGE TO_EOF");

	/*
	 * By i_disk_size file1 is now a one-block file, so this looks like a
	 * full-contents exchange of two one-block files and
	 * xmi_can_exchange_reflink_flags() clears the flag.
	 */
	if (exchange(f1, d2, 0, 0, blksz, 0, "exchange flag-clear"))
		return 2;
	show("3 EXCHANGE_RANGE");

	/* in-place write to the still-shared block */
	buf = malloc(blksz);
	memset(buf, 'X', blksz);
	if (pwrite(f1, buf, blksz, blksz) != (ssize_t)blksz)
		return fprintf(stderr, "pwrite file1: %m\n"), 2;
	fsync(f1);
	show("4 pwrite(file1, blksz)");

	posix_fadvise(peer, 0, 0, POSIX_FADV_DONTNEED);
	if (pread(peer, after, blksz, 0) != (ssize_t)blksz)
		return fprintf(stderr, "pread peer: %m\n"), 2;
	printf("\npeer first byte %c -> %c   %s\n", before[0], after[0],
	       memcmp(before, after, blksz) ? "PEER REWRITTEN" : "peer intact");
	return memcmp(before, after, blksz) ? 0 : 1;
}
