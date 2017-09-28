#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <sys/mman.h>

void
err_exit(char *op)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errno));
	exit(1);
}

int
main(int argc, char **argv)
{
	int fd, pfd, ret;
	char *buf;
	/*
	 * gcc -O2 will optimize foo's storage, prevent reproducing
	 * this issue.
	 * foo is never actually used after fault in value stored.
	 */
	volatile char foo __attribute__((__unused__));
	int pagesize = getpagesize();

	if (argc < 3) {
		printf("Usage: %s <file> <pmem file>\n", basename(argv[0]));
		exit(0);
	}

	 /* O_DIRECT is necessary for reproduce this bug. */
	fd = open(argv[1], O_RDONLY|O_DIRECT);
	if (fd < 0)
		err_exit("open");

	pfd = open(argv[2], O_RDONLY);
	if (pfd < 0)
		err_exit("pmem open");

	buf = mmap(NULL, pagesize, PROT_READ, MAP_SHARED, pfd, 0);
	if (buf == MAP_FAILED)
		err_exit("mmap");

	/*
	 * Read from the DAX mmap to populate the first page in the
	 * address_space with a read-only mapping.
	 */
	foo = *buf;

	/*
	 * Now write to the DAX mmap.  This *should* fail, but if the bug is
	 * present in __get_user_pages_fast(), it will succeed.
	 */
	ret = read(fd, buf, pagesize);
	if (ret != pagesize)
		err_exit("read");

	ret = msync(buf, pagesize, MS_SYNC);
	if (ret != 0)
		err_exit("msync");

	ret = munmap(buf, pagesize);
	if (ret < 0)
		err_exit("munmap");

	ret = close(fd);
	if (ret < 0)
		err_exit("clsoe fd");

	ret = close(pfd);
	if (ret < 0)
		err_exit("close pfd");

	exit(0);
}
