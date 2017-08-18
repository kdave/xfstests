/*
 * Copyright (c) 2013 Alibaba Group.
 * Copyright (c) 2017 Red Hat Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * This is a normal case that we do some append dio writes and meanwhile
 * we do some dio reads.  Currently in vfs we don't ensure that i_size
 * is updated properly.  Hence the reader will read some data with '0'.
 * But we expect that the reader should read nothing or data with 'a'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <libaio.h>
#include <pthread.h>

struct io_data {
	int fd;
	size_t blksize;
	off_t offset;
	char *buf;
	int use_aio;
};

int reader_ready = 0;

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-a] <file>\n", prog);
	fprintf(stderr, "\t-a\tuse aio-dio\n");
	exit(EXIT_FAILURE);
}

static void *reader(void *arg)
{
	struct io_data *data = (struct io_data *)arg;
	int ret;

	memset(data->buf, 'b', data->blksize);
	reader_ready = 1;
	do {
		ret = pread(data->fd, data->buf, data->blksize, data->offset);
		if (ret < 0)
			perror("read file");
	} while (ret <= 0);

	return NULL;
}

static void *writer(struct io_data *data)
{
	int ret;

	while (!reader_ready)
		usleep(1);

	if (data->use_aio) {
		struct io_context *ctx = NULL;
		struct io_event evs[1];
		struct iocb iocb;
		struct iocb *iocbs[] = { &iocb };

		ret = io_setup(1, &ctx);
		if (ret) {
			fprintf(stderr, "error %s during io_setup\n",
				strerror(ret));
			return NULL;
		}
		io_prep_pwrite(&iocb, data->fd, data->buf, data->blksize, data->offset);
		ret = io_submit(ctx, 1, iocbs);
		if (ret != 1) {
			fprintf(stderr, "error %s during io_submit\n",
				strerror(ret));
			return NULL;
		}
		ret = io_getevents(ctx, 1, 1, evs, NULL);
		if (ret != 1) {
			fprintf(stderr, "error %s during io_getevents\n",
				strerror(ret));
			return NULL;
		}
	} else {
		ret = pwrite(data->fd, data->buf, data->blksize, data->offset);
		if (ret < 0)
			perror("write file failed");
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t tid;
	struct io_data wdata;
	struct io_data rdata;
	size_t max_blocks = 128;		/* 128 */
	size_t blksize = 1 * 1024 * 1024;	/* 1M */
	char *rbuf = NULL, *wbuf = NULL;
	int rfd = 0, wfd = 0;
	int i, j, c;
	int use_aio = 1;
	int ret = 0;
	int io_align = 4096;
	char *prog;
	char *testfile;


	prog = basename(argv[0]);

	while ((c = getopt(argc, argv, "a:d")) != -1) {
		switch (c) {
		case 'a':
			io_align = strtol(optarg, NULL, 0);
			break;
		case 'd':
			use_aio = 0;
			break;
		default:
			usage(prog);
		}
	}
	if (optind != argc - 1)
		usage(prog);
	testfile = argv[optind];

	wfd = open(testfile, O_CREAT|O_DIRECT|O_WRONLY|O_TRUNC, 0644);
	if (wfd < 0) {
		perror("open for write");
		exit(1);
	}

	rfd = open(testfile, O_DIRECT|O_RDONLY, 0644);
	if (rfd < 0) {
		perror("open for read");
		ret = 1;
		goto err;
	}

	ret = posix_memalign((void **)&wbuf, io_align, blksize);
	if (ret) {
		fprintf(stderr, "failed to alloc memory: %s\n", strerror(ret));
		ret = 1;
		goto err;
	}

	ret = posix_memalign((void **)&rbuf, io_align, blksize);
	if (ret) {
		fprintf(stderr, "failed to alloc memory: %s\n", strerror(ret));
		ret = 1;
		goto err;
	}

	memset(wbuf, 'a', blksize);
	wdata.fd = wfd;
	wdata.blksize = blksize;
	wdata.buf = wbuf;
	wdata.use_aio = use_aio;
	rdata.fd = rfd;
	rdata.blksize = blksize;
	rdata.buf = rbuf;

	for (i = 0; i < max_blocks; i++) {
		wdata.offset = rdata.offset = i * blksize;

		/* reset reader_ready, it will be set in reader thread */
		reader_ready = 0;
		ret = pthread_create(&tid, NULL, reader, &rdata);
		if (ret) {
			fprintf(stderr, "create reader thread failed: %s\n",
				strerror(ret));
			ret = 1;
			goto err;
		}

		writer(&wdata);

		ret = pthread_join(tid, NULL);
		if (ret) {
			fprintf(stderr, "pthread join reader failed: %s\n",
				strerror(ret));
			ret = 1;
			goto err;
		}

		for (j = 0; j < blksize; j++) {
			if (rdata.buf[j] != 'a') {
				fprintf(stderr, "encounter an error: "
					"block %d offset %d, content %x\n",
					i, j, rbuf[j]);
				ret = 1;
				goto err;
			}
		}
	}

err:
	if (rfd)
		close(rfd);
	if (wfd)
		close(wfd);
	if (rbuf)
		free(rbuf);
	if (wbuf)
		free(wbuf);

	exit(ret);
}
