#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static pthread_barrier_t barrier;

static unsigned long extent_size;
static unsigned long num_extents;

struct dio_thread_data {
	int fd;
	int thread_id;
};

static void *dio_thread(void *arg)
{
	struct dio_thread_data *data = arg;
	off_t off;
	ssize_t ret;
	void *buf;

	if ((errno = posix_memalign(&buf, extent_size / 2, extent_size / 2))) {
		perror("malloc");
		return NULL;
	}
	memset(buf, 0, extent_size / 2);

	off = (num_extents - 1) * extent_size;
	if (data->thread_id)
		off += extent_size / 2;
	while (off >= 0) {
		pthread_barrier_wait(&barrier);

		ret = pread(data->fd, buf, extent_size / 2, off);
		if (ret == -1)
			perror("pread");

		off -= extent_size;
	}

	free(buf);
	return NULL;
}

int main(int argc, char **argv)
{
	struct dio_thread_data data[2];
	pthread_t thread;
	int fd;

	if (argc != 4) {
		fprintf(stderr, "usage: %s EXTENT_SIZE NUM_EXTENTS PATH\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	extent_size = strtoul(argv[1], NULL, 0);
	num_extents = strtoul(argv[2], NULL, 0);

	errno = pthread_barrier_init(&barrier, NULL, 2);
	if (errno) {
		perror("pthread_barrier_init");
		return EXIT_FAILURE;
	}

	fd = open(argv[3], O_RDONLY | O_DIRECT);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	data[0].fd = fd;
	data[0].thread_id = 0;
	errno = pthread_create(&thread, NULL, dio_thread, &data[0]);
	if (errno) {
		perror("pthread_create");
		close(fd);
		return EXIT_FAILURE;
	}

	data[1].fd = fd;
	data[1].thread_id = 1;
	dio_thread(&data[1]);

	pthread_join(thread, NULL);

	close(fd);
	return EXIT_SUCCESS;
}
