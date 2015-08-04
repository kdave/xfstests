#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <xfs/xfs.h>

/* test thanks to judith@sgi.com */

#define IO_SIZE	1048576

void
print_getbmapx(
	const char	*pathname,
	int		fd,
	int64_t		start,
	int64_t		limit);

int
main(int argc, char *argv[])
{
	int i;
	int fd;
	char *buf;
	struct dioattr dio;
	xfs_flock64_t flock;
	off_t offset;
	char	*file;
	int	loops;
	char	*dio_env;

	if(argc != 3) {
		fprintf(stderr, "%s <loops> <file>\n", argv[0]);
		exit(1);
	}

	errno = 0;
	loops = strtoull(argv[1], NULL, 0);
	if (errno) {
		perror("strtoull");
		exit(errno);
	}
	file = argv[2];

	while (loops-- > 0) {
		sleep(1);
		fd = open(file, O_RDWR|O_CREAT|O_DIRECT, 0666);
		if (fd < 0) {
			perror("open");
			exit(1);
		}
		if (xfsctl(file, fd, XFS_IOC_DIOINFO, &dio) < 0) {
			perror("dioinfo");
			exit(1);
		}

		dio_env = getenv("XFS_DIO_MIN");
		if (dio_env)
			dio.d_mem = dio.d_miniosz = atoi(dio_env);

		if ((dio.d_miniosz > IO_SIZE) || (dio.d_maxiosz < IO_SIZE)) {
			fprintf(stderr, "Test won't work - iosize out of range"
				" (dio.d_miniosz=%d, dio.d_maxiosz=%d)\n",
				dio.d_miniosz, dio.d_maxiosz);

			exit(1);
		}
		buf = (char *)memalign(dio.d_mem , IO_SIZE);
		if (buf == NULL) {
			fprintf(stderr,"Can't get memory\n");
			exit(1);
		}
		memset(buf,'Z',IO_SIZE);
		offset = 0;

		flock.l_whence = 0;
		flock.l_start= 0;
		flock.l_len = IO_SIZE*21;
		if (xfsctl(file, fd, XFS_IOC_RESVSP64, &flock) < 0) {
			perror("xfsctl ");
			exit(1);
		}
		for (i = 0; i < 21; i++) {
			if (pwrite(fd, buf, IO_SIZE, offset) != IO_SIZE) {
				perror("pwrite");
				exit(1);
			}
			offset += IO_SIZE;
		}

		print_getbmapx(file, fd, 0, 0);

		flock.l_whence = 0;
		flock.l_start= 0;
		flock.l_len = 0;
		xfsctl(file, fd, XFS_IOC_FREESP64, &flock);
		print_getbmapx(file, fd, 0, 0);
		close(fd);
	}
	exit(0);
}

void
print_getbmapx(
const   char	*pathname,
	int	fd,
	int64_t	start,
	int64_t	limit)
{
	struct getbmapx bmapx[50];
	int array_size = sizeof(bmapx) / sizeof(bmapx[0]);
	int x;
	int foundone = 0;
	int foundany = 0;

again:
	foundone = 0;
	memset(bmapx, '\0', sizeof(bmapx));

	bmapx[0].bmv_offset = start;
	bmapx[0].bmv_length = -1; /* limit - start; */
	bmapx[0].bmv_count = array_size;
	bmapx[0].bmv_entries = 0;       /* no entries filled in yet */

	bmapx[0].bmv_iflags = BMV_IF_PREALLOC;

	x = array_size;
	for (;;) {
		if (x > bmapx[0].bmv_entries) {
			if (x != array_size) {
				break;  /* end of file */
			}
			if (xfsctl(pathname, fd, XFS_IOC_GETBMAPX, bmapx) < 0) {
				fprintf(stderr, "XFS_IOC_GETBMAPX failed\n");
				exit(1);
			}
			if (bmapx[0].bmv_entries == 0) {
				break;
			}
			x = 1;  /* back at first extent in buffer */
		}
		if (bmapx[x].bmv_oflags & 1) {
			fprintf(stderr, "FOUND ONE %lld %lld %x\n",
				(long long) bmapx[x].bmv_offset,
				(long long) bmapx[x].bmv_length,
				bmapx[x].bmv_oflags);
			foundone = 1;
			foundany = 1;
		}
		x++;
	}
	if (foundone) {
		sleep(1);
		fprintf(stderr,"Repeat\n");
		goto again;
	}
	if (foundany) {
		exit(1);
	}
}

