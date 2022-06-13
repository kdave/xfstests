// setattr.c by kanda.motohiro@gmail.com
// xfs extended attribute corruption bug reproducer
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <linux/limits.h>

#define die() do { perror(""); \
fprintf(stderr, "error at line %d\n", __LINE__); \
exit(1); } while (0)

#define fail(...) do { \
fprintf(stderr, __VA_ARGS__); exit (1); \
} while (0)

void usage(char *progname)
{
	fail("usage: %s [-m max_attr_size] <file>\n", progname);
}

int main(int argc, char *argv[])
{
	int ret;
	int fd;
	int c;
	char *path;
	char *name = "user.world";
	char *value;
	struct stat sbuf;
	size_t size = sizeof(value);
	size_t maxsize = XATTR_SIZE_MAX;

	while ((c = getopt(argc, argv, "m:")) != -1) {
		char *endp;

		switch (c) {
		case 'm':
			maxsize = strtoul(optarg, &endp, 0);
			if (*endp || (maxsize > XATTR_SIZE_MAX))
				fail("Invalid 'max_attr_size' value\n");
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind == argc - 1)
		path = argv[optind];
	else
		usage(argv[0]);

	fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) die();

	/*
	 * The value should be 3/4 the size of a fs block to ensure that we
	 * get to extents format.
	 */
	ret = fstat(fd, &sbuf);
	if (ret < 0) die();
	size = sbuf.st_blksize * 3 / 4;
	if (!size)
		fail("Invalid st_blksize(%ld)\n", sbuf.st_blksize);
	size = MIN(size, maxsize);
	value = malloc(size);
	if (!value)
		fail("Failed to allocate memory\n");

	/* First, create a small xattr. */
	memset(value, '0', 1);
	ret = fsetxattr(fd, name, value, 1, XATTR_CREATE);
	if (ret < 0) die();
	close(fd);

	fd = open(path, O_RDWR);
	if (fd < 0) die();

	/* Then, replace it with bigger one, forcing short form to leaf conversion. */
	memset(value, '1', size);
	ret = fsetxattr(fd, name, value, size, XATTR_REPLACE);
	if (ret < 0) die();
	close(fd);

	return 0;
}
