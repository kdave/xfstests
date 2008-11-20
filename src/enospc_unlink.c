/* 
 * Write a file until filesystem full is reached and then unlink it.
 * Demonstrates a particular deadlock that Kaz hit in testing.  Run
 * several iterations of this in parallel on a near-full filesystem.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	int			fd, sz, done;
	void			*ptr;
	size_t			written;
	unsigned int		i, count = 0;
	unsigned long long	bytes = 0;

	if (argc < 2) {
		fprintf(stderr, "Insufficient arguments\n");
		exit(1);
	}
	if (argc < 3) {
		count = (unsigned int)atoi(argv[2]);
	}

	sz = 1024 * 1024;

	ptr = memalign(sz, sz);
	if (!ptr) {
		perror("memalign");
		exit(1);
	}
	memset(ptr, 0xffffffff, sz);

	for (i = 0; i < count; i++) {
		fd = open(argv[1], O_CREAT|O_WRONLY, 0666);
		if (fd < 0) {
			perror(argv[1]);
			exit(1);
		}

		done = 0;
		while (!done) {
			written = write(fd, ptr, sz);
			if (written == -1) {
				if (errno == ENOSPC) {
					close(fd);
					unlink(argv[1]);
					printf("Unlinked %s\n", argv[1]);
					done = 1;
				}
				else {
					printf("Skipped unlink %s (%s)\n",
						argv[1], strerror(errno));
					done = -1;
				}
			}
			if (written == 0) {
				printf("Wrote zero bytes?\n");
				done = -1;
			}
			bytes += written;
		}
		if (done > 0)
			printf("Wrote %llu bytes total.\n", bytes);
		else
			exit(1);
	}
	printf("Completed %u iterations of unlink/out-of-space test.\n", i);
	exit(0);
}
