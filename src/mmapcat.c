/* mmapcat.c - derived from source by misiek@pld.ORG.PL */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int fd;
	char *ptr;
	struct stat64 st;

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		exit(1);
	}
	fstat64(fd, &st);
	ptr = mmap64(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	while (*ptr != 0)
		write(1, ptr++, 1);
	exit(0);
}
