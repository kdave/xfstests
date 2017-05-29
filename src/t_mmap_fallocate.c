#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int fd;
	char *buf;
	size_t fsize, i;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file> <size-in-KB>\n",
			basename(argv[0]));
		exit(1);
	}

	fsize = strtol(argv[2], NULL, 10);
	if (fsize <= 0) {
		fprintf(stderr, "Invalid file size: %s\n", argv[2]);
		exit(1);
	}
	fsize <<= 10;

	fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror("Cannot open file");
	        exit(1);
	}

	buf = mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("Cannot memory map");
		exit(1);
	}

	/*
	 * We extend the file byte-by-byte through fallocate(2) and write data
	 * to each byte through the mmap. Then we verify whether the data is
	 * really there to see whether the zeroing of last file page during
	 * writeback didn't corrupt the data.
	 */
	for (i = 0; i < fsize; i++) {
		if (posix_fallocate(fd, i, 1) != 0) {
			perror("Cannot fallocate");
			exit(1);
		}
		buf[i] = 0x78;

		if (buf[i] != 0x78) {
			fprintf(stderr,
				"Value not written correctly (off=%lu)\n",
				(unsigned long)i);
			exit(1);
		}
	}

	for (i = 0; i < fsize; i++) {
		if (buf[i] != 0x78) {
			fprintf(stderr, "Value has been modified (off=%lu)\n",
				(unsigned long)i);
			exit(1);
		}
	}

	return 0;
}
