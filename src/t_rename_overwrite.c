#include <stdio.h>
#include <fcntl.h>
#include <err.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	const char *path1;
	const char *path2;
	struct stat stbuf;
	int res;
	int fd;

	if (argc != 3) {
		fprintf(stderr, "usage: %s path1 path2\n", argv[0]);
		return 1;
	}

	path1 = argv[1];
	path2 = argv[2];
	fd = open(path2, O_RDONLY);
	if (fd == -1)
		err(1, "open(\"%s\")", path2);

	res = rename(path1, path2);
	if (res == -1)
		err(1, "rename(\"%s\", \"%s\")", path1, path2);

	res = fstat(fd, &stbuf);
	if (res == -1)
		err(1, "fstat(%i)", fd);

	if (stbuf.st_nlink != 0) {
		fprintf(stderr, "nlink is %lu, should be 0\n",
			(unsigned long) stbuf.st_nlink);
		return 1;
	}

	return 0;
}
