/*
 * t_encrypted_d_revalidate
 *
 * Test that ->d_revalidate() for encrypted dentries doesn't oops the
 * kernel by incorrectly not dropping out of RCU mode.  To do this, try
 * to look up a negative dentry while another thread deletes its parent
 * directory.  Fixed by commit 03a8bb0e53d9 ("ext4/fscrypto: avoid RCU
 * lookup in d_revalidate").
 *
 * This doesn't always reproduce reliably, but we give it a few seconds.
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2017 Google, Inc.  All Rights Reserved.
 *
 * Author: Eric Biggers <ebiggers@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licence as published by
 * the Free Software Foundation; either version 2 of the Licence, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TIMEOUT		10
#define DIR_NAME	"dir"
#define FILE_NAME	DIR_NAME "/file"

static volatile sig_atomic_t timed_out = 0;

static void alarm_handler(int sig)
{
	timed_out = 1;
}

static void __attribute__((noreturn))
die(int err, const char *msg)
{
	fprintf(stderr, "ERROR: %s", msg);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fputc('\n', stderr);
	exit(1);
}

static void *stat_thread(void *_arg)
{
	struct stat stbuf;

	for (;;) {
		if (stat(FILE_NAME, &stbuf) == 0)
			die(0, "stat should have failed");
		if (errno != ENOENT)
			die(errno, "stat");
	}
}

int main(int argc, char *argv[])
{
	long ncpus;
	long num_stat_threads;
	long i;
	struct stat stbuf;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s DIR\n", argv[0]);
		return 2;
	}

	if (chdir(argv[1]) != 0)
		die(errno, "chdir");

	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus > 1)
		num_stat_threads = ncpus - 1;
	else
		num_stat_threads = 1;

	for (i = 0; i < num_stat_threads; i++) {
		pthread_t thread;
		int err;

		err = pthread_create(&thread, NULL, stat_thread, NULL);
		if (err)
			die(err, "pthread_create");
	}

	if (signal(SIGALRM, alarm_handler) == SIG_ERR)
		die(errno, "signal");

	alarm(TIMEOUT);

	while (!timed_out) {
		if (mkdir(DIR_NAME, 0777) != 0)
			die(errno, "mkdir");
		if (stat(FILE_NAME, &stbuf) == 0)
			die(0, "stat should have failed");
		if (errno != ENOENT)
			die(errno, "stat");
		if (rmdir(DIR_NAME) != 0)
			die(errno, "rmdir");
	}

	printf("t_encrypted_d_revalidate finished\n");
	return 0;
}
