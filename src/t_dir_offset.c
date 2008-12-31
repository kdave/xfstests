
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>

struct linux_dirent64 {
	uint64_t	d_ino;
	int64_t		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[0];
};


#define BUF_SIZE 4096

int
main(int argc, char *argv[])
{
	int fd, nread;
	char buf[BUF_SIZE];
	struct linux_dirent64 *d;
	int bpos;

	fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	for ( ; ; ) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1) {
			perror("getdents");
			exit(EXIT_FAILURE);
		}

		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread;) {
			d = (struct linux_dirent64 *) (buf + bpos);
			/*
			 * Can't use off_t here xfsqa is compiled with
			 * -D_FILE_OFFSET_BITS=64
			 */
			if (d->d_off != (long)d->d_off) {
				fprintf(stderr, "detected d_off truncation "
						"d_name = %s, d_off = %lld\n",
						d->d_name, (long long)d->d_off);
				exit(EXIT_FAILURE);
			}
			bpos += d->d_reclen;
		}
	}

	exit(EXIT_SUCCESS);
}
