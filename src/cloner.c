/*
 *  Tiny program to perform file (range) clones using raw Btrfs and CIFS ioctls.
 *
 *  Copyright (C) 2014 SUSE Linux Products GmbH. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/magic.h>
#ifdef HAVE_BTRFS_IOCTL_H
#include <btrfs/ioctl.h>
#else

struct btrfs_ioctl_clone_range_args {
	int64_t src_fd;
	uint64_t src_offset;
	uint64_t src_length;
	uint64_t dest_offset;
};

#define BTRFS_IOCTL_MAGIC 0x94
#define BTRFS_IOC_CLONE       _IOW(BTRFS_IOCTL_MAGIC, 9, int)
#define BTRFS_IOC_CLONE_RANGE _IOW(BTRFS_IOCTL_MAGIC, 13, \
				   struct btrfs_ioctl_clone_range_args)
#endif

#ifdef HAVE_CIFS_IOCTL_H
#include <cifs/ioctl.h>
#else

#define CIFS_IOCTL_MAGIC 0xCF
#define CIFS_IOC_COPYCHUNK_FILE _IOW(CIFS_IOCTL_MAGIC, 3, int)

#endif

#ifndef BTRFS_SUPER_MAGIC
#define BTRFS_SUPER_MAGIC    0x9123683E
#endif
#ifndef CIFS_MAGIC_NUMBER
#define CIFS_MAGIC_NUMBER    0xFE534D42
#endif

static void
usage(char *name, const char *msg)
{
	printf("Fatal: %s\n"
	       "Usage:\n"
	       "%s [options] <src_file> <dest_file>\n"
	       "\tA full file clone is performed by default, "
	       "unless any of the following are specified (Btrfs only):\n"
	       "\t-s <offset>:	source file offset (default = 0)\n"
	       "\t-d <offset>:	destination file offset (default = 0)\n"
	       "\t-l <length>:	length of clone (default = 0)\n\n"
	       "\tBoth Btrfs and CIFS are supported. On Btrfs, a COW clone "
	       "is attempted. On CIFS, a server-side copy is requested.\n",
	       msg, name);
	_exit(1);
}

static int
clone_file_btrfs(int src_fd, int dst_fd)
{
	int ret = ioctl(dst_fd, BTRFS_IOC_CLONE, src_fd);
	if (ret != 0)
		ret = errno;
	return ret;
}

static int
clone_file_cifs(int src_fd, int dst_fd)
{
	int ret = ioctl(dst_fd, CIFS_IOC_COPYCHUNK_FILE, src_fd);
	if (ret != 0)
		ret = errno;
	return ret;
}

static int
clone_file(unsigned int fs_type, int src_fd, int dst_fd)
{
	switch (fs_type) {
	case BTRFS_SUPER_MAGIC:
		return clone_file_btrfs(src_fd, dst_fd);
		break;
	case CIFS_MAGIC_NUMBER:
		return clone_file_cifs(src_fd, dst_fd);
		break;
	default:
		return ENOTSUP;
		break;
	}
}

static int
clone_file_range_btrfs(int src_fd, int dst_fd, uint64_t src_off,
		       uint64_t dst_off, uint64_t len)
{
	struct btrfs_ioctl_clone_range_args cr_args;
	int ret;

	memset(&cr_args, 0, sizeof(cr_args));
	cr_args.src_fd = src_fd;
	cr_args.src_offset = src_off;
	cr_args.src_length = len;
	cr_args.dest_offset = dst_off;
	ret = ioctl(dst_fd, BTRFS_IOC_CLONE_RANGE, &cr_args);
	if (ret != 0)
		ret = errno;
	return ret;
}

static int
clone_file_range(unsigned int fs_type, int src_fd, int dst_fd, uint64_t src_off,
		 uint64_t dst_off, uint64_t len)
{
	switch (fs_type) {
	case BTRFS_SUPER_MAGIC:
		return clone_file_range_btrfs(src_fd, dst_fd, src_off, dst_off,
					      len);
		break;
	case CIFS_MAGIC_NUMBER:	/* only supports full file server-side copies */
	default:
		return ENOTSUP;
		break;
	}
}

static int
cloner_check_fs_support(int src_fd, int dest_fd, unsigned int *fs_type)
{
	int ret;
	struct statfs sfs;

	ret = fstatfs(src_fd, &sfs);
	if (ret != 0) {
		printf("failed to stat source FS\n");
		return errno;
	}

	if ((sfs.f_type != BTRFS_SUPER_MAGIC)
	 && (sfs.f_type != CIFS_MAGIC_NUMBER)) {
		printf("unsupported source FS 0x%x\n",
		       (unsigned int)sfs.f_type);
		return ENOTSUP;
	}

	*fs_type = (unsigned int)sfs.f_type;

	ret = fstatfs(dest_fd, &sfs);
	if (ret != 0) {
		printf("failed to stat destination FS\n");
		return errno;
	}

	if (sfs.f_type != *fs_type) {
		printf("dest FS type 0x%x does not match source 0x%x\n",
		       (unsigned int)sfs.f_type, *fs_type);
		return ENOTSUP;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	bool full_file = true;
	uint64_t src_off = 0;
	uint64_t dst_off = 0;
	uint64_t len = 0;
	char *src_file;
	int src_fd;
	char *dst_file;
	int dst_fd;
	int ret;
	int opt;
	unsigned int fs_type = 0;

	while ((opt = getopt(argc, argv, "s:d:l:")) != -1) {
		char *sval_end;
		switch (opt) {
		case 's':
			errno = 0;
			src_off = strtoull(optarg, &sval_end, 10);
			if ((errno) || (*sval_end != '\0'))
				usage(argv[0], "invalid source offset");
			full_file = false;
			break;
		case 'd':
			errno = 0;
			dst_off = strtoull(optarg, &sval_end, 10);
			if ((errno) || (*sval_end != '\0'))
				usage(argv[0], "invalid destination offset");
			full_file = false;
			break;
		case 'l':
			errno = 0;
			len = strtoull(optarg, &sval_end, 10);
			if ((errno) || (*sval_end != '\0'))
				usage(argv[0], "invalid length");
			full_file = false;
			break;
		default:
			usage(argv[0], "invalid argument");
		}
	}

	/* should be exactly two args left */
	if (optind != argc - 2)
		usage(argv[0], "src_file and dst_file arguments are madatory");

	src_file = (char *)strdup(argv[optind++]);
	if (src_file == NULL) {
		ret = ENOMEM;
		printf("no memory\n");
		goto err_out;
	}
	dst_file = (char *)strdup(argv[optind++]);
	if (dst_file == NULL) {
		ret = ENOMEM;
		printf("no memory\n");
		goto err_src_free;
	}

	src_fd = open(src_file, O_RDONLY);
	if (src_fd == -1) {
		ret = errno;
		printf("failed to open %s: %s\n", src_file, strerror(errno));
		goto err_dst_free;
	}
	dst_fd = open(dst_file, O_CREAT | O_WRONLY, 0644);
	if (dst_fd == -1) {
		ret = errno;
		printf("failed to open %s: %s\n", dst_file, strerror(errno));
		goto err_src_close;
	}

	ret = cloner_check_fs_support(src_fd, dst_fd, &fs_type);
	if (ret != 0) {
		goto err_dst_close;
	}

	if (full_file) {
		ret = clone_file(fs_type, src_fd, dst_fd);
	} else {
		ret = clone_file_range(fs_type, src_fd, dst_fd, src_off,
				       dst_off, len);
	}
	if (ret != 0) {
		printf("clone failed: %s\n", strerror(ret));
		goto err_dst_close;
	}

	ret = 0;
err_dst_close:
	if (close(dst_fd)) {
		ret |= errno;
		printf("failed to close dst file: %s\n", strerror(errno));
	}
err_src_close:
	if (close(src_fd)) {
		ret |= errno;
		printf("failed to close src file: %s\n", strerror(errno));
	}
err_dst_free:
	free(dst_file);
err_src_free:
	free(src_file);
err_out:
	return ret;
}
