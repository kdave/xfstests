// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Fujitsu Limited.  All Rights Reserved. */
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <time.h>
#include <unistd.h>

sem_t *sem;

void sigbus_handler(int signal)
{
	printf("Process is killed by signal: %d\n", signal);
	sem_post(sem);
}

void mmap_read_file(char *filename, off_t offset, size_t size)
{
	int fd;
	char *map, *dummy;
	struct timespec ts;

	fd = open(filename, O_RDWR);
	map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, offset);
	dummy = malloc(size);

	/* make sure page fault happens */
	memcpy(dummy, map, size);

	/* ready */
	sem_post(sem);

	usleep(200000);

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 3;
	/* wait for injection done */
	sem_timedwait(sem, &ts);

	free(dummy);
	munmap(map, size);
	close(fd);
}

void mmap_read_file_then_poison(char *filename, off_t offset, size_t size,
		off_t poisonOffset, size_t poisonSize)
{
	int fd, error;
	char *map, *dummy;

	/* wait for parent preparation done */
	sem_wait(sem);

	fd = open(filename, O_RDWR);
	map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, offset);
	dummy = malloc(size);

	/* make sure page fault happens */
	memcpy(dummy, map, size);

	printf("Inject poison...\n");
	error = madvise(map + poisonOffset, poisonSize, MADV_HWPOISON);
	if (error)
		printf("madvise() has fault: %d, errno: %d\n", error, errno);

	free(dummy);
	munmap(map, size);
	close(fd);
}

int main(int argc, char *argv[])
{
	char *pReadFile = NULL, *pPoisonFile = NULL;
	size_t mmapSize, poisonSize;
	off_t mmapOffset = 0, poisonOffset = 0;
	long pagesize = sysconf(_SC_PAGESIZE);
	int c;
	pid_t pid;

	if (pagesize < 1) {
		fprintf(stderr, "sysconf(_SC_PAGESIZE): failed to get page size\n");
		abort();
	}

	/* default mmap / poison size, in unit of System Page Size */
	mmapSize = poisonSize = pagesize;

	while ((c = getopt(argc, argv, "o::s::O::S::R:P:")) != -1) {
		switch (c) {
		/* mmap offset */
		case 'o':
			mmapOffset = atoi(optarg) * pagesize;
			break;
		/* mmap size */
		case 's':
			mmapSize = atoi(optarg) * pagesize;
			break;
		/* madvice offset */
		case 'O':
			poisonOffset = atoi(optarg) * pagesize;
			break;
		/* madvice size */
		case 'S':
			poisonSize = atoi(optarg) * pagesize;
			break;
		/* filename for mmap read */
		case 'R':
			pReadFile = optarg;
			break;
		/* filename for poison read */
		case 'P':
			pPoisonFile = optarg;
			break;
		default:
			printf("Unknown option: %c\n", c);
			exit(1);
		}
	}

	if (!pReadFile || !pPoisonFile) {
		printf("Usage: \n"
		       "  %s [-o mmapOffset] [-s mmapSize] [-O mmapOffset] [-S mmapSize] -R readFile -P poisonFile\n"
		       "  (offset and size are both in unit of System Page Size: %ld)\n",
				basename(argv[0]), pagesize);
		exit(0);
	}
	if (poisonSize < mmapSize)
		mmapSize = poisonSize;

	/* fork and mmap files */
	pid = fork();
	if (pid == 0) {
		/* handle SIGBUS */
		signal(SIGBUS, sigbus_handler);
		sem = sem_open("sync", O_CREAT, 0666, 0);

		/* mread & do memory failure on poison file */
		mmap_read_file_then_poison(pPoisonFile, mmapOffset, mmapSize,
				poisonOffset, poisonSize);

		sem_close(sem);
	} else {
		sem = sem_open("sync", O_CREAT, 0666, 0);

		/* mread read file, wait for child process to be killed */
		mmap_read_file(pReadFile, mmapOffset, mmapSize);
		sem_close(sem);
	}
	exit(0);
}
