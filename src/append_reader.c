/* Simple test program for checking O_APPEND writes (see append_writer.c)
 *
 * Contributed by hatakeyama@bsd.tnes.nec.co.jp
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	char *file;
	int fd, i, rc, d;

	if (argc < 2)
		exit(1);

	file = argv[1];

	if ((fd = open(file, O_RDONLY, 0600)) == -1) {
		perror("couldn't open");
		exit(1);
	}

	for (i = 0; ;i ++) {
		if ((rc = read(fd, &d, sizeof(d))) != sizeof(i)) {
			if (rc == 0)
				exit(0);
			perror("couldn't read");
			exit(1);
		}

		if (d != i) {
			fprintf(stderr, "bad data, offset = %u", i * 4);
			exit(1);
		}
	}

	exit(0);
}

