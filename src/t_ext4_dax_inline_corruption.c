#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PAGE(a) ((a)*0x1000)
#define STRLEN 256

void err_exit(char *op)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errno));
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd, err, len = PAGE(1);
	char *dax_data, *data;
	char string[STRLEN];

	if (argc < 2) {
		printf("Usage: %s <file>\n", basename(argv[0]));
		exit(0);
	}

	srand(time(NULL));
	snprintf(string, STRLEN, "random number %d\n", rand());

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		err_exit("fd");

	data = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		err_exit("mmap data");

	/* this fallocate turns off inline data and turns on DAX */
	fallocate(fd, 0, 0, PAGE(2));

	dax_data = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (dax_data == MAP_FAILED)
		err_exit("mmap dax_data");

	/*
	 * Write the data using the non-DAX mapping, and try and read it back
	 * using the DAX mapping.
	 */
	strcpy(data, string);
	if (strcmp(dax_data, string) != 0)
		printf("Data miscompare\n");

	err = munmap(dax_data, len);
	if (err < 0)
		err_exit("munmap dax_data");

	err = munmap(data, len);
	if (err < 0)
		err_exit("munmap data");

	err = close(fd);
	if (err < 0)
		err_exit("close");
	return 0;
}
