// SPDX-License-Identifier: GPL-2.0
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"

ssize_t read_nointr(int fd, void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = read(fd, buf, count);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

ssize_t write_nointr(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = write(fd, buf, count);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static int write_file(const char *path, const void *buf, size_t count)
{
	int fd;
	ssize_t ret;

	fd = open(path, O_WRONLY | O_CLOEXEC | O_NOCTTY | O_NOFOLLOW);
	if (fd < 0)
		return -1;

	ret = write_nointr(fd, buf, count);
	close(fd);
	if (ret < 0 || (size_t)ret != count)
		return -1;

	return 0;
}

static int map_ids(pid_t pid, unsigned long nsid, unsigned long hostid,
		   unsigned long range)
{
	char map[100], procfile[256];

	snprintf(procfile, sizeof(procfile), "/proc/%d/uid_map", pid);
	snprintf(map, sizeof(map), "%lu %lu %lu", nsid, hostid, range);
	if (write_file(procfile, map, strlen(map)))
		return -1;


	snprintf(procfile, sizeof(procfile), "/proc/%d/gid_map", pid);
	snprintf(map, sizeof(map), "%lu %lu %lu", nsid, hostid, range);
	if (write_file(procfile, map, strlen(map)))
		return -1;

	return 0;
}

#define __STACK_SIZE (8 * 1024 * 1024)
pid_t do_clone(int (*fn)(void *), void *arg, int flags)
{
	void *stack;

	stack = malloc(__STACK_SIZE);
	if (!stack)
		return -ENOMEM;

#ifdef __ia64__
	return __clone2(fn, stack, __STACK_SIZE, flags | SIGCHLD, arg, NULL);
#else
	return clone(fn, stack + __STACK_SIZE, flags | SIGCHLD, arg, NULL);
#endif
}

int get_userns_fd_cb(void *data)
{
	return kill(getpid(), SIGSTOP);
}

int get_userns_fd(unsigned long nsid, unsigned long hostid, unsigned long range)
{
	int ret;
	pid_t pid;
	char path[256];

	pid = do_clone(get_userns_fd_cb, NULL, CLONE_NEWUSER);
	if (pid < 0)
		return -errno;

	ret = map_ids(pid, nsid, hostid, range);
	if (ret < 0)
		return ret;

	snprintf(path, sizeof(path), "/proc/%d/ns/user", pid);
	ret = open(path, O_RDONLY | O_CLOEXEC);
	kill(pid, SIGKILL);
	wait_for_pid(pid);
	return ret;
}

int wait_for_pid(pid_t pid)
{
	int status, ret;

again:
	ret = waitpid(pid, &status, 0);
	if (ret == -1) {
		if (errno == EINTR)
			goto again;

		return -1;
	}

	if (!WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}
