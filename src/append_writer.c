/* Simple test program for O_APPEND writes (checked by append_reader.c)
 * 
 * Contributed by hatakeyama@bsd.tnes.nec.co.jp
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>

int main(int argc, char **argv)
{
	char file[MAXPATHLEN];
	int fd, i, iterations;

	if (argc < 2)
		exit(1);

	iterations = atoi(argv[1]);

	sprintf(file, "testfile.%d", getpid());

	if ((fd = open(file, O_CREAT | O_RDWR | O_APPEND, 0600)) == -1) {
		perror("couldn't open");
		exit(1);
	}

	for (i = 0; i < iterations;i++) {
		if (write(fd, &i, sizeof(i)) != sizeof(i)) {
			perror("couldn't write");
			exit(1);
		}
	}

	exit(0);
}

