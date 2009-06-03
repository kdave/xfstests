/*
 *   aio-dio-invalidate-readahead - test sync DIO invalidation of readahead
 *   Copyright (C) 2007 Zach Brown
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#define _XOPEN_SOURCE 500 /* pwrite */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libaio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <malloc.h>

/*
 * sync DIO invalidates the read cache after it finishes the write.  This
 * is to invalidate cached pages which might have been brought in during
 * the write.
 *
 * In http://lkml.org/lkml/2007/10/26/478 a user reported this failing
 * for his case of readers and writers racing.  It turned out that his
 * reader wasn't actually racing with the writer, but read-ahead from
 * the reader pushed reads up into the region that the writer was working
 * on.
 *
 * This test reproduces his case.  We have a writing thread tell
 * a reading thread how far into the file it will find new data.
 * The reader reads behind the writer, checking for stale data.
 * If the kernel fails to invalidate the read-ahead after the
 * write then the reader will see stale data.
 */
#ifndef O_DIRECT
#define O_DIRECT         040000 /* direct disk access hint */
#endif

#define FILE_SIZE (8 * 1024 * 1024)

/* this test always failed before 10 seconds on a single spindle */
#define SECONDS 30

#define fail(fmt , args...) do {\
	printf(fmt , ##args);	\
	exit(1);		\
} while (0)

int page_size;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
loff_t write_pos = 0;
loff_t read_pos = 0;
unsigned char byte = 0;

static void *writer(void *arg)
{
	char *path = arg;
	loff_t off;
	void *buf;
	int ret;
	int fd;
	time_t start = time(NULL);

	buf = memalign(page_size, page_size);
	if (buf == NULL)
		fail("failed to allocate an aligned page");

	fd = open(path, O_DIRECT|O_CREAT|O_RDWR|O_TRUNC, 0644);
	if (fd < 0)
		fail("dio open failed: %d\n", errno);

	while (1) {
		if ((time(NULL) - start) > SECONDS) {
			printf("test ran for %u seconds without error\n",
			       SECONDS);
			exit(0);
		}

		pthread_mutex_lock(&mut);
		while (read_pos != write_pos)
			pthread_cond_wait(&cond, &mut);
		byte++;
		write_pos = 0;
		pthread_mutex_unlock(&mut);

		memset(buf, byte, page_size);

		for (off = 0; off < FILE_SIZE; off += page_size) {

			ret = pwrite(fd, buf, page_size, off);
			if (ret != page_size)
				fail("write returned %d", ret);

			if ((rand() % 4) == 0) {
				pthread_mutex_lock(&mut);
				write_pos = off;
				pthread_cond_signal(&cond);
				pthread_mutex_unlock(&mut);
			};
		}
	}
}

static void *reader(void *arg)
{
	char *path = arg;
	unsigned char old;
	loff_t read_to = 0;
	void *found;
	int fd;
	int ret;
	void *buf;
	loff_t off;

	setvbuf(stdout, NULL, _IONBF, 0);

	buf = memalign(page_size, page_size);
	if (buf == NULL)
		fail("failed to allocate an aligned page");

	fd = open(path, O_CREAT|O_RDONLY, 0644);
	if (fd < 0)
		fail("buffered open failed: %d\n", errno);

	while (1) {
		pthread_mutex_lock(&mut);
		read_pos = read_to;
		pthread_cond_signal(&cond);
		while (read_pos == write_pos)
			pthread_cond_wait(&cond, &mut);
		read_to = write_pos;
		off = read_pos;
		old = byte - 1;
		pthread_mutex_unlock(&mut);

		for (; off < read_to; off += page_size) {

			ret = pread(fd, buf, page_size, off);
			if (ret != page_size)
				fail("write returned %d", ret);

			found = memchr(buf, old, page_size);
			if (found)
				fail("reader found old byte at pos %lu",
				     (unsigned long)off +
				     (unsigned long)found -
				     (unsigned long)buf);
		}
	}
}

int main(int argc, char **argv)
{
	pthread_t reader_thread;
	pthread_t writer_thread;
	int ret;

	page_size = getpagesize();

	if (argc != 2)
		fail("only arg should be file name");

	ret = pthread_create(&writer_thread, NULL, writer, argv[1]);
	if (ret == 0)
		ret = pthread_create(&reader_thread, NULL, reader, argv[1]);
	if (ret)
		fail("failed to start reader and writer threads: %d", ret);

	pthread_join(writer_thread, NULL);
	pthread_join(reader_thread, NULL);
	exit(0);
}
