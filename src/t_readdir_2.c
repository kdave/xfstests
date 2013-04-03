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
		int ret1, i;
		static int array[11] = {0, 1, 2, 3, 1023, 1024, 1025, 4095,
					4096, 4097, 0x7fffffff};

		while(1) {
			for(i = 0; i < 11; i++) {
				ret1 = lseek(fd, array[i++], SEEK_SET);
				if (ret1 == -1) {
					perror("Seek failed!");
					exit(EXIT_FAILURE);
				}
			}

			off_t pos = lseek(fd, 0, SEEK_CUR);
			lseek(fd, pos, SEEK_SET);
		}
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
