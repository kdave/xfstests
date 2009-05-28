#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <xfs/xfs.h>
#include <sys/mman.h>

/*
 * mmap a preallocated file and write to a set of offsets
 * in it. We'll check to see if the underlying extents are
 * converted correctly on writeback.
 */
int main(int argc, char **argv) {
	unsigned long long o;
	int fd, i;
	struct xfs_flock64 space;
	unsigned char *buf;

	if(argc < 3) {
		fprintf(stderr, "%s <count> <file [file...]>\n", argv[0]);
		exit(1);
	}

	errno = 0;
	o = strtoull(argv[1], NULL, 0);
	if (errno) {
		perror("strtoull");
		exit(errno);
	}

	for(i = 2; i < argc; i++) {
		unlink(argv[i]);
		fd = open(argv[i], O_RDWR|O_CREAT|O_LARGEFILE, 0666);
		if(fd < 0) {
			perror("open");
			exit(2);
		}

		if(ftruncate64(fd, o) < 0) {
			perror("ftruncate64");
			exit(3);
		}

		space.l_whence = SEEK_SET;
		space.l_start = 0;
		space.l_len = o;

		if(ioctl(fd, XFS_IOC_RESVSP64, &space)) {
			perror("ioctl()");
			exit(4);
		}

		buf = mmap(NULL, (int)o, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		if(buf == MAP_FAILED) {
			perror("mmap()");
			exit(5);
		} else {
			buf[o-1] = 0;
			buf[o/2] = 0;
			buf[0] = 0;
			munmap(buf, o);
		}

		fsync(fd);
		if(close(fd) == -1) {
			perror("close");
			exit(6);
		}
	}
	exit(0);
}
