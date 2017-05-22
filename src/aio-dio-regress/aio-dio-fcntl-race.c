/*
 * Perform aio writes to file and toggle O_DIRECT flag concurrently
 * this may trigger race between file->f_flags read and modification
 * unuligned aio allow to makes race window wider.
 * Regression test for https://lkml.org/lkml/2014/10/8/545 CVE-2014-8086
 * Patch proposed: http://www.spinics.net/lists/linux-ext4/msg45683.html
 *
 * Copyright (c) 2014 Dmitry Monakhov.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libaio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE	512
#define LOOP_SECONDS 10


static int do_aio_loop(int fd, void *buf)
{
	int err, ret;
	struct io_context *ctx = NULL;
	struct io_event ev;
	struct iocb iocb, *iocbs[] = { &iocb };
	struct timeval start, now, delta = { 0, 0 };

	ret = 0;
	err = io_setup(1, &ctx);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(-err), "io_setup" );
		return 1;
	}
	gettimeofday(&start, NULL);
	while (1) {
		io_prep_pwrite(&iocb, fd, buf, BUF_SIZE, BUF_SIZE);
		err = io_submit(ctx, 1, iocbs);
		if (err != 1) {
			fprintf(stderr, "error %s during %s\n",
				strerror(-err),
				"io_submit");
			ret = 1;
			break;
		}
		err = io_getevents(ctx, 1, 1, &ev, NULL);
		if (err != 1) {
			fprintf(stderr, "error %s during %s\n",
				strerror(-err),
				"io_getevents");
			ret = 1;
			break;
		}
		gettimeofday(&now, NULL);
		timersub(&now, &start, &delta);
		if (delta.tv_sec >= LOOP_SECONDS)
			break;
	}
	io_destroy(ctx);
	return ret;
}

int main(int argc, char **argv)
{
	int flags, fd;
	int pid1, pid2 = 0;
	int ret1, ret = 0;

	if (argc != 2){
		printf("Usage %s fname\n", argv[0]);
		return 1;
	}
	fd = open(argv[1], O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	pid1 = fork();
	if (pid1 < 0) {
		perror("fork");
		return 1;
	}

	if (pid1 == 0) {
		struct timeval start, now, delta = { 0, 0 };

		gettimeofday(&start, NULL);

		/* child: toggle O_DIRECT*/
		flags = fcntl(fd, F_GETFL);
		while (1) {
			ret = fcntl(fd, F_SETFL, flags | O_DIRECT);
			if (ret) {
				perror("fcntl O_DIRECT");
				return 1;
			}
			ret = fcntl(fd, F_SETFL, flags);
			if (ret) {
				perror("fcntl");
				return 1;
			}
			gettimeofday(&now, NULL);
			timersub(&now, &start, &delta);
			if (delta.tv_sec >= LOOP_SECONDS)
				break;
		}
	} else {
		/* parent: AIO */
		void *buf = NULL;
		int err;

		err = posix_memalign(&buf, BUF_SIZE, BUF_SIZE);
		if (err || buf == NULL) {
			fprintf(stderr, "posix_memalign failed: %s\n",
				strerror(err));
			exit(1);
		}
		/* Two tasks which performs unaligned aio will be serialized
		   which maks race window wider */
		pid2 = fork();
		if (pid2 < 0)
			goto out;
		else if (pid2 > 0)
			printf("All tasks are spawned\n");

		ret = do_aio_loop(fd, buf);
	}
out:
	/* Parent wait for all others */
	if (pid2 > 0){
		waitpid(pid1, &ret1, 0);
		if (!ret)
			ret = ret1;
		waitpid(pid2, &ret1, 0);
	} else {
		waitpid(pid1, &ret1, 0);
	}
	if (!ret)
		ret = ret1;

	return ret;
}
