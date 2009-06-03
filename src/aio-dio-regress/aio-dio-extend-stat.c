/*
 *   aio-dio-extend-stat - test race in dio aio completion
 *   Copyright (C) 2006 Rafal Wijata
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

#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libaio.h>
#include <malloc.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#ifndef O_DIRECT
#define O_DIRECT         040000 /* direct disk access hint */
#endif

/*
 * This was originally submitted to
 * http://bugzilla.kernel.org/show_bug.cgi?id=6831 by 
 * Rafal Wijata <wijata@nec-labs.com>.  It caught a race in dio aio completion
 * that would call aio_complete() before the dio callers would update i_size.
 * A stat after io_getevents() would not see the new file size.
 *
 * The bug was fixed in the fs/direct-io.c completion reworking that appeared
 * in 2.6.20.  This test should fail on 2.6.19.
 */

#define BUFSIZE 1024
int bufsize;

static unsigned char buf[BUFSIZE] __attribute((aligned (4096)));

/* 
 * this was arbitrarily chosen to take about two seconds on a dual athlon in a
 * debugging kernel.. it trips up long before that.
 */
#define MAX_AIO_EVENTS 4000

#define fail(fmt , args...) do {\
	printf(fmt , ##args);	\
	exit(1);		\
} while (0)

void fun_write1(void* ptr);
void fun_writeN(void* ptr);
void fun_read(void* ptr);

int  handle = 0;
io_context_t ctxp;
struct iocb *iocbs[MAX_AIO_EVENTS];
struct io_event ioevents[MAX_AIO_EVENTS];

volatile int submittedSize = 0; //synchronization

int get_sector_size(dev_t dev)
{
	int fd, sector_size;
	char *nodename = "bogus_node";
	
	if (mknod(nodename, S_IFBLK|0600, dev) != 0) {
		if (errno == EPERM)
			printf("This test requires root priviledges\n");
		fail("mknod failed with %d\n", errno);
	}

	fd = open(nodename, O_RDONLY);
	if (fd < 0)
		fail("failed to open device node: %d\n", errno);

	if (ioctl(fd, BLKSSZGET, &sector_size) < 0)
		fail("BLKSSZGET failed: %d\n", errno);
	close(fd);
	unlink(nodename);

	return sector_size;
}

int main(int argc, char **argv)
{
	pthread_t thread_read; 
	pthread_t thread_write;
	int i;
	int ret;
	int sector_size;
	struct stat st;

	if (argc != 2)
		fail("only arg should be file name\n");

	handle = open(argv[1], O_CREAT | O_TRUNC | O_DIRECT | O_RDWR, 0600); 
	if (handle == -1) 
		fail("failed to open test file %s, errno: %d\n",
			argv[1], errno);

	if (fstat(handle, &st) == -1)
		fail("initial stat of empty file failed with %d\n", errno);

	sector_size = get_sector_size(st.st_dev);
	if (sector_size < 0)
		fail("failed to retrieve the sector size of the device\n");

	/*
	 * When this test was written, it used a buffer size of 1024 bytes.  It is
	 * not clear to me at this time whether that was significant.  It may have
	 * been easier to trigger the problem when writing less than the file system
	 * block size, for example, or less than the system page size.  To try to
	 * limit this change in scope to those platforms that have a device with a
	 * larger underlying sector size, the following bogosity is used.  ;-)
	 * -JEM
	 */
	if (sector_size > 512)
		bufsize = sector_size;
	else
		bufsize = BUFSIZE;

	for (i = 0; i < bufsize; ++i)
		buf[i] = 'A' + (char)(i % ('Z'-'A'+1));

	buf[bufsize-1] = '\n';

	memset(&ctxp, 0, sizeof(ctxp));
	ret = io_setup(MAX_AIO_EVENTS, &ctxp);
	if (ret)
		fail("io_setup returned %d\n", ret);

	for (i = 0; i < MAX_AIO_EVENTS; ++i) {

		iocbs[i] = calloc(1, sizeof(struct iocb));
		if (iocbs[i] == NULL)
			fail("failed to allocate an iocb\n");
	
/*		iocbs[i]->data = i; */
		iocbs[i]->aio_fildes = handle;
		iocbs[i]->aio_lio_opcode = IO_CMD_PWRITE;
		iocbs[i]->aio_reqprio = 0;
		iocbs[i]->u.c.buf = buf;
		iocbs[i]->u.c.nbytes = bufsize;
		iocbs[i]->u.c.offset = bufsize*i;
	}

	pthread_create(&thread_read, NULL, (void*)&fun_read, NULL);
	pthread_create(&thread_write, NULL, (void*)&fun_writeN, NULL);

	pthread_join(thread_read, NULL);
	pthread_join(thread_write, NULL);

	io_destroy(ctxp);
	close(handle);

	printf("%u iterations of racing extensions and collection passed\n",
		MAX_AIO_EVENTS);

	return 0;
}

void fun_read(void *ptr)
{
	long n = MAX_AIO_EVENTS;
	struct stat filestat;
	long long exSize;
	long i;
	long r;

	while (n > 0) {
		r = io_getevents(ctxp, 1, MAX_AIO_EVENTS, ioevents, NULL);
		if (r < 0) 
			fail("io_getevents returned %ld\n", r);

		n -= r;
		for (i = 0; i < r; ++i) {
			struct io_event *event = &ioevents[i];
			if (event->res != bufsize)
				fail("error in block: expected %d bytes, "
				     "received %ld\n", bufsize,
				     event->obj->u.c.nbytes);

			exSize = event->obj->u.c.offset + event->obj->u.c.nbytes;
			fstat(handle, &filestat);
			if (filestat.st_size < exSize)
				fail("write of %lu bytes @%llu finished, "
				     "expected filesize at least %llu, but "
				     "got %ld\n", event->obj->u.c.nbytes,
				     event->obj->u.c.offset, exSize, filestat.st_size);
		}
	}
}

void fun_writeN(void *ptr)
{
	int i;
	int ret;

	for(i = 0; i < MAX_AIO_EVENTS; ++i) {
		ret = io_submit(ctxp, 1, &(iocbs[i]));
		if (ret != 1)
			fail("io_subit returned %d instead of 1\n", ret);
	}
}

void fun_write1(void *ptr)
{
	int ret;
    
	ret = io_submit(ctxp, MAX_AIO_EVENTS, iocbs);
	if (ret !=  MAX_AIO_EVENTS)
		fail("io_subit returned %d instead of %u\n", ret,
		     MAX_AIO_EVENTS);
}
