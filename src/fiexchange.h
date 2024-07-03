/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
/*
 * FIEXCHANGE ioctl definitions, to facilitate exchanging parts of files.
 *
 * Copyright (C) 2022 Oracle.  All Rights Reserved.
 *
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef _LINUX_FIEXCHANGE_H
#define _LINUX_FIEXCHANGE_H

#include <linux/types.h>

/*
 * Exchange part of file1 with part of the file that this ioctl that is being
 * called against (which we'll call file2).  Filesystems must be able to
 * restart and complete the operation even after the system goes down.
 */
struct xfs_exchange_range {
	__s32		file1_fd;
	__u32		pad;		/* must be zeroes */
	__u64		file1_offset;	/* file1 offset, bytes */
	__u64		file2_offset;	/* file2 offset, bytes */
	__u64		length;		/* bytes to exchange */

	__u64		flags;		/* see XFS_EXCHANGE_RANGE_* below */
};

/*
 * Exchange file data all the way to the ends of both files, and then exchange
 * the file sizes.  This flag can be used to replace a file's contents with a
 * different amount of data.  length will be ignored.
 */
#define XFS_EXCHANGE_RANGE_TO_EOF	(1ULL << 0)

/* Flush all changes in file data and file metadata to disk before returning. */
#define XFS_EXCHANGE_RANGE_DSYNC	(1ULL << 1)

/* Dry run; do all the parameter verification but do not change anything. */
#define XFS_EXCHANGE_RANGE_DRY_RUN	(1ULL << 2)

/*
 * Exchange only the parts of the two files where the file allocation units
 * mapped to file1's range have been written to.  This can accelerate
 * scatter-gather atomic writes with a temp file if all writes are aligned to
 * the file allocation unit.
 */
#define XFS_EXCHANGE_RANGE_FILE1_WRITTEN (1ULL << 3)

#define XFS_EXCHANGE_RANGE_ALL_FLAGS	(XFS_EXCHANGE_RANGE_TO_EOF | \
					 XFS_EXCHANGE_RANGE_DSYNC | \
					 XFS_EXCHANGE_RANGE_DRY_RUN | \
					 XFS_EXCHANGE_RANGE_FILE1_WRITTEN)

#define XFS_IOC_EXCHANGE_RANGE	     _IOW ('X', 129, struct xfs_exchange_range)

#endif /* _LINUX_FIEXCHANGE_H */
