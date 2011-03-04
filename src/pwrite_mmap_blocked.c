/*    Copyright (c) 2010 Intel Corporation
 *
 *    This program is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the Free
 *    Software Foundation; version 2 of the License
 *
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *    for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc., 59
 *    Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/stat.h>



int main(int argc, char *argv[])
{
	int ret;
	char *cc = "01234";
	char *progname;
	loff_t size;
	loff_t amount = 1;
	loff_t from = 2;
	loff_t to = 3;
	int fd;
	void *mapped_mem;

	progname = argv[0];
	size = 5;
	fd = open(argv[1], O_RDWR|O_TRUNC|O_CREAT, 0666);
	if (fd < 0) {
		fprintf(stderr, "%s: Cannot open `%s': %s\n",
			progname, argv[1], strerror(errno));
		exit(1);
	}

	if ((ret = pwrite(fd, (const char *)cc,
				size, 0)) != size) {
		perror("pwrite");
		exit(1);
	}

	mapped_mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapped_mem == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	printf("pwrite %Ld bytes from %Ld to %Ld\n",
		(long long) amount, (long long) from, (long long) to);

	ret = pwrite(fd, (char *)mapped_mem + from, amount, to);
	if (ret != amount) {
		perror("pwrite");
		exit(1);
	}

	munmap(mapped_mem,0);
	close(fd);
	exit(0);
}
