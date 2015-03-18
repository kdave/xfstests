/*
 * Copyright (c) 2014, Miklos Szeredi <mszeredi@suse.cz>
 * This file is published under GPL2+.
 *
 * This is a trivial wrapper around the renameat2 syscall.
 */

#include "global.h"

#ifndef HAVE_RENAMEAT2
#include <sys/syscall.h>

#if !defined(SYS_renameat2) && defined(__x86_64__)
#define SYS_renameat2 316
#endif

#if !defined(SYS_renameat2) && defined(__i386__)
#define SYS_renameat2 353
#endif

#if !defined(SYS_renameat2) && defined(__i386__)
#define SYS_renameat2 353
#endif

static int renameat2(int dfd1, const char *path1,
		     int dfd2, const char *path2,
		     unsigned int flags)
{
#ifdef SYS_renameat2
	return syscall(SYS_renameat2, dfd1, path1, dfd2, path2, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}
#endif

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE	(1 << 0)	/* Don't overwrite target */
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE		(1 << 1)	/* Exchange source and dest */
#endif
#ifndef RENAME_WHITEOUT
#define RENAME_WHITEOUT		(1 << 2)	/* Whiteout source */
#endif

int main(int argc, char *argv[])
{
	int ret;
	int c;
	const char *path1 = NULL;
	const char *path2 = NULL;
	unsigned int flags = 0;
	int test = 0;

	for (c = 1; c < argc; c++) {
		if (argv[c][0] == '-') {
			switch (argv[c][1]) {
			case 't':
				test = 1;
				break;
			case 'n':
				flags |= RENAME_NOREPLACE;
				break;
			case 'x':
				flags |= RENAME_EXCHANGE;
				break;
			case 'w':
				flags |= RENAME_WHITEOUT;
				break;
			default:
				goto usage;
			}
		} else if (!path1) {
			path1 = argv[c];
		} else if (!path2) {
			path2 = argv[c];
		} else {
			goto usage;
		}
	}

	if (!test && (!path1 || !path2))
		goto usage;

	ret = renameat2(AT_FDCWD, path1, AT_FDCWD, path2, flags);
	if (ret == -1) {
		if (test) {
			if (errno == ENOSYS || errno == EINVAL)
				return 1;
			else
				return 0;
		}
		/*
		 * Turn EEXIST into ENOTEMPTY.  E.g. XFS uses EEXIST, and that
		 * is also accepted by the standards.
		 *
		 * This applies only to plain rename and RENAME_WHITEOUT
		 */
		if (errno == EEXIST && (!flags || (flags & RENAME_WHITEOUT)))
			errno = ENOTEMPTY;

		perror("");
		return 1;
	}

	return 0;

usage:
	fprintf(stderr,
		"usage: %s [-t] [-n|-x|-w] path1 path2\n"
		"  -t  test\n"
		"  -n  noreplace\n"
		"  -x  exchange\n"
		"  -w  whiteout\n", argv[0]);

	return 1;
}
