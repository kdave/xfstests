#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MiB(a) ((a)*1024*1024)

void err_exit(char *op)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errno));
	exit(1);
}

int main(int argc, char *argv[])
{
	volatile int a __attribute__((__unused__));
	char *buffer = "HELLO WORLD!";
	char *data;
	int fd, err, ret = 0;

	if (argc < 2) {
		printf("Usage: %s <pmem file>\n", basename(argv[0]));
		exit(0);
	}

	fd = open(argv[1], O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd < 0)
		err_exit("fd");

	/*
	 * This allows us to map a huge zero page, and we do it at a non-zero
	 * offset for a little additional testing.
	 */
	ftruncate(fd, 0);
	ftruncate(fd, MiB(4));

	data = mmap(NULL, MiB(2), PROT_READ, MAP_SHARED, fd, MiB(2));

	/*
	 * This faults in a 2MiB zero page to satisfy the read.
	 * 'a' is volatile so this read doesn't get optimized out.
	 */
	a = data[0];

	pwrite(fd, buffer, strlen(buffer), MiB(2));

	/*
	 * Try and use the mmap to read back the data we just wrote with
	 * pwrite().  If the kernel bug is present the mapping from the 2MiB
	 * zero page will still be intact, and we'll read back zeros instead.
	 */
	if (strncmp(buffer, data, strlen(buffer))) {
		fprintf(stderr, "strncmp mismatch: '%s' vs '%s'\n", buffer,
				data);
		ret = 1;
	}

	err = munmap(data, MiB(2));
	if (err < 0)
		err_exit("munmap");

	err = close(fd);
	if (err < 0)
		err_exit("close");

	return ret;
}
