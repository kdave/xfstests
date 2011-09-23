/*
 * Copyright (c) 2011 Gražvydas Ignotas
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

/*
 * This test checks if no duplicate d_off values are returned and
 * that these offsets are seekable to entry with the right inode.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
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

int
main(int argc, char *argv[])
{
	int fd, nread;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;
	int bpos, total, i;
	off_t lret;
	int retval = EXIT_SUCCESS;

	fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	total = 0;
	for ( ; ; ) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1) {
			perror("getdents");
			exit(EXIT_FAILURE);
		}

		if (nread == 0)
			break;

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
			bpos += d->d_reclen;
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

		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
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
