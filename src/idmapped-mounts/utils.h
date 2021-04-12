/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __IDMAP_UTILS_H
#define __IDMAP_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <linux/types.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "missing.h"

extern pid_t do_clone(int (*fn)(void *), void *arg, int flags);
extern int get_userns_fd(unsigned long nsid, unsigned long hostid,
			 unsigned long range);
extern ssize_t read_nointr(int fd, void *buf, size_t count);
extern int wait_for_pid(pid_t pid);
extern ssize_t write_nointr(int fd, const void *buf, size_t count);

#endif /* __IDMAP_UTILS_H */
