/*
 * Copyright (C) 2016 CTERA Networks. All Rights Reserved.
 * Author: Amir Goldstein <amir73il@gmail.com>
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
 * t_dir_type
 *
 * print directory entries and their file type, optionally filtered by d_type
 * or by inode number.
 *
 * ./t_dir_type <path> [u|f|d|c|b|l|p|s|w|<ino>]
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/syscall.h>

struct linux_dirent64 {
	uint64_t	d_ino;
	int64_t		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[0];
};

#define DT_MASK 15
#define DT_MAX 15
unsigned char type_to_char[DT_MAX] = {
	[DT_UNKNOWN] = 'u',
	[DT_DIR] = 'd',
	[DT_REG] = 'f',
	[DT_LNK] = 'l',
	[DT_CHR] = 'c',
	[DT_BLK] = 'b',
	[DT_FIFO] = 'p',
	[DT_SOCK] = 's',
	[DT_WHT] = 'w',
};

#define DT_CHAR(t) type_to_char[(t)&DT_MASK]

#define BUF_SIZE 4096

int
main(int argc, char *argv[])
{
	int fd, nread;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;
	int bpos;
	int type = -1; /* -1 means all types */
	uint64_t ino = 0;
	int ret = 1;

	fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	if (argc > 2 && argv[2][0]) {
		char t = argv[2][0];

		for (type = DT_MAX-1; type >= 0; type--)
			if (DT_CHAR(type) == t)
				break;
		/* no match ends up with type = -1 */
		if (type < 0)
			ino = atoll(argv[2]);
	}

	for ( ; ; ) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1) {
			perror("getdents");
			exit(EXIT_FAILURE);
		}

		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread;) {
			d = (struct linux_dirent64 *) (buf + bpos);
			if ((type < 0 || type == (int)d->d_type) &&
			    (!ino || ino == d->d_ino)) {
				ret = 0;
				printf("%s %c\n", d->d_name, DT_CHAR(d->d_type));
			}
			bpos += d->d_reclen;
		}
	}

	return ret;
}
