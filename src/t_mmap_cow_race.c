#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MiB(a) ((a)*1024*1024)
#define NUM_THREADS 2

void err_exit(char *op)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errno));
	exit(1);
}

void worker_fn(void *ptr)
{
	char *data = (char *)ptr;
	volatile int a;
	int i, err;

	for (i = 0; i < 10; i++) {
		a = data[0];
		data[0] = a;

		err = madvise(data, MiB(2), MADV_DONTNEED);
		if (err < 0)
			err_exit("madvise");

		/* Mix up the thread timings to encourage the race. */
		err = usleep(rand() % 100);
		if (err < 0)
			err_exit("usleep");
	}
}

int main(int argc, char *argv[])
{
	pthread_t thread[NUM_THREADS];
	int i, j, fd, err;
	char *data;

	if (argc < 2) {
		printf("Usage: %s <file>\n", basename(argv[0]));
		exit(0);
	}

	fd = open(argv[1], O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd < 0)
		err_exit("fd");

	/* This allows us to map a huge page. */
	ftruncate(fd, 0);
	ftruncate(fd, MiB(2));

	/*
	 * First we set up a shared mapping.  Our write will (hopefully) get
	 * the filesystem to give us a 2MiB huge page DAX mapping.  We will
	 * then use this 2MiB page for our private mapping race.
	 */
	data = mmap(NULL, MiB(2), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		err_exit("shared mmap");

	data[0] = 1;

	err = munmap(data, MiB(2));
	if (err < 0)
		err_exit("shared munmap");

	for (i = 0; i < 500; i++) {
		data = mmap(NULL, MiB(2), PROT_READ|PROT_WRITE, MAP_PRIVATE,
				fd, 0);
		if (data == MAP_FAILED)
			err_exit("private mmap");

		for (j = 0; j < NUM_THREADS; j++) {
			err = pthread_create(&thread[j], NULL,
					(void*)&worker_fn, data);
			if (err)
				err_exit("pthread_create");
		}

		for (j = 0; j < NUM_THREADS; j++) {
			err = pthread_join(thread[j], NULL);
			if (err)
				err_exit("pthread_join");
		}

		err = munmap(data, MiB(2));
		if (err < 0)
			err_exit("private munmap");
	}

	err = close(fd);
	if (err < 0)
		err_exit("close");

	return 0;
}
