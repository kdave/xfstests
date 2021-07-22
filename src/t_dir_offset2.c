// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2011 Gražvydas Ignotas
 */

/*
 * This test checks if no duplicate d_off values are returned and
 * that these offsets are seekable to entry with the right inode.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>

struct linux_dirent64 {
	uint64_t	d_ino;
	uint64_t	d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[0];
};

#define BUF_SIZE 4096
#define HISTORY_LEN 1024

static uint64_t d_off_history[HISTORY_LEN];
static uint64_t d_ino_history[HISTORY_LEN];

void usage()
{
	fprintf(stderr, "usage: t_dir_offset2: <dir> [[bufsize] [-|+]<filename> [-v]]\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int fd, fd2 = -1;
	char buf[BUF_SIZE];
	int nread, bufsize = BUF_SIZE;
	struct linux_dirent64 *d;
	struct stat st = {};
	int bpos, total, i;
	off_t lret;
	int retval = EXIT_SUCCESS;
	const char *filename = NULL;
	int exists = 0, found = 0;
	int modify = 0, verbose = 0;

	if (argc > 2) {
		bufsize = atoi(argv[2]);
		if (!bufsize)
			usage();
		if (bufsize > BUF_SIZE)
			bufsize = BUF_SIZE;

		if (argc > 3) {
			filename = argv[3];
			/* +<filename> creates, -<filename> removes */
			if (filename[0] == '+')
				modify = 1;
			else if (filename[0] == '-')
				modify = -1;
			if (modify)
				filename++;
			if (argc > 4 && !strcmp(argv[4], "-v"))
				verbose = 1;
		}
	} else if (argc < 2) {
		usage();
	}

	fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	if (filename) {
		exists = !fstatat(fd, filename, &st, AT_SYMLINK_NOFOLLOW);
		if (!exists && errno != ENOENT) {
			perror("fstatat");
			exit(EXIT_FAILURE);
		}
	}

	total = 0;
	for ( ; ; ) {
		nread = syscall(SYS_getdents64, fd, buf, bufsize);
		if (nread == -1) {
			perror("getdents");
			exit(EXIT_FAILURE);
		}

		if (modify && fd2 < 0 && total == 0) {
			printf("getdents at offset 0 returned %d bytes\n", nread);

			/* create/unlink entry after first getdents */
			if (modify > 0) {
				if (openat(fd, filename, O_CREAT, 0600) < 0) {
					perror("openat");
					exit(EXIT_FAILURE);
				}
				exists = 1;
				printf("created entry %s\n", filename);
			} else if (modify < 0) {
				if (unlinkat(fd, filename, 0) < 0) {
					perror("unlinkat");
					exit(EXIT_FAILURE);
				}
				exists = 0;
				printf("unlinked entry %s\n", filename);
			}

			/*
			 * Old fd may not return new entry and may return stale
			 * entries which is allowed.  Keep old fd open and open
			 * a new fd to check for stale or missing entries later.
			 */
			fd2 = open(argv[1], O_RDONLY | O_DIRECTORY);
			if (fd2 < 0) {
				perror("open fd2");
				exit(EXIT_FAILURE);
			}
		}

		if (nread == 0) {
			if (fd2 < 0 || fd == fd2)
				break;

			/* Re-iterate with new fd leaving old fd open */
			fd = fd2;
			total = 0;
			found = 0;
			continue;
		}

		for (bpos = 0; bpos < nread; total++) {
			d = (struct linux_dirent64 *) (buf + bpos);

			if (total >= HISTORY_LEN) {
				fprintf(stderr, "too many files\n");
				break;
			}

			for (i = 0; i < total; i++)
			{
				if (d_off_history[i] == d->d_off) {
					fprintf(stderr, "entries %d and %d have duplicate d_off %lld\n",
						i, total, (long long int)d->d_off);
					retval = EXIT_FAILURE;
				}
			}
			d_off_history[total] = d->d_off;
			d_ino_history[total] = d->d_ino;
			if (filename) {
				if (verbose)
					printf("entry #%d: %s (d_ino=%lld, d_off=%lld)\n",
					       i, d->d_name, (long long int)d->d_ino,
					       (long long int)d->d_off);
				if (!strcmp(filename, d->d_name)) {
					found = 1;
					if (st.st_ino && d->d_ino != st.st_ino) {
						fprintf(stderr, "entry %s has inconsistent d_ino (%lld != %lld)\n",
								filename,
								(long long int)d->d_ino,
								(long long int)st.st_ino);
					}
				}

			}
			bpos += d->d_reclen;
		}
	}

	if (filename) {
		if (exists == found) {
			printf("entry %s %sfound as expected\n", filename, found ? "" : "not ");
		} else {
			fprintf(stderr, "%s entry %s\n",
				exists ? "missing" : "stale", filename);
			exit(EXIT_FAILURE);
		}
	}

	/* check if seek works correctly */
	d = (struct linux_dirent64 *)buf;
	for (i = total - 1; i >= 0; i--)
	{
		lret = lseek(fd, i > 0 ? d_off_history[i - 1] : 0, SEEK_SET);
		if (lret == -1) {
			perror("lseek");
			exit(EXIT_FAILURE);
		}

		nread = syscall(SYS_getdents64, fd, buf, bufsize);
		if (nread == -1) {
			perror("getdents");
			exit(EXIT_FAILURE);
		}

		if (nread == 0) {
			fprintf(stderr, "getdents returned 0 on entry %d\n", i);
			retval = EXIT_FAILURE;
		}

		if (d->d_ino != d_ino_history[i]) {
			fprintf(stderr, "entry %d has inode %lld, expected %lld\n",
				i, (long long int)d->d_ino, (long long int)d_ino_history[i]);
			retval = EXIT_FAILURE;
		}
	}

	close(fd);
	exit(retval);
}
