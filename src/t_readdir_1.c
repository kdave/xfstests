#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
	int fd;
	int ret;
	DIR *dir;
	struct dirent *ptr;

	dir = opendir(argv[1]);

	fd = dirfd(dir);
	if (fd < 0) {
		perror("Failed to get dirfd!");
		exit(EXIT_FAILURE);
	}
	ret = fork();

	if (ret == 0) {
		char buf[100];

		while(1)
                        read(fd, buf, 100);

	} else {
		while (1) {
			int ret2 = lseek(fd, 0, SEEK_SET);
			if (ret2 == -1) {
				perror("Seek failed!");
				exit(EXIT_FAILURE);
			}
			while ((ptr = readdir(dir)))
					;
		}
	}

	closedir(dir);
	exit(EXIT_SUCCESS);
}
