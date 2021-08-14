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

/* A few helpful macros. */
#define STRLITERALLEN(x) (sizeof(""x"") - 1)

#define INTTYPE_TO_STRLEN(type)             \
	(2 + (sizeof(type) <= 1             \
		  ? 3                       \
		  : sizeof(type) <= 2       \
			? 5                 \
			: sizeof(type) <= 4 \
			      ? 10          \
			      : sizeof(type) <= 8 ? 20 : sizeof(int[-2 * (sizeof(type) > 8)])))

#define syserror(format, ...)                           \
	({                                              \
		fprintf(stderr, "%m - " format "\n", ##__VA_ARGS__); \
		(-errno);                               \
	})

#define syserror_set(__ret__, format, ...)                    \
	({                                                    \
		typeof(__ret__) __internal_ret__ = (__ret__); \
		errno = labs(__ret__);                        \
		fprintf(stderr, "%m - " format "\n", ##__VA_ARGS__);       \
		__internal_ret__;                             \
	})

typedef enum idmap_type_t {
	ID_TYPE_UID,
	ID_TYPE_GID
} idmap_type_t;

struct id_map {
	idmap_type_t map_type;
	__u32 nsid;
	__u32 hostid;
	__u32 range;
};

struct list {
	void *elem;
	struct list *next;
	struct list *prev;
};

#define list_for_each(__iterator, __list) \
	for (__iterator = (__list)->next; __iterator != __list; __iterator = __iterator->next)

static inline void list_init(struct list *list)
{
	list->elem = NULL;
	list->next = list->prev = list;
}

static inline int list_empty(const struct list *list)
{
	return list == list->next;
}

static inline void __list_add(struct list *new, struct list *prev, struct list *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add_tail(struct list *head, struct list *list)
{
	__list_add(list, head->prev, head);
}

extern pid_t do_clone(int (*fn)(void *), void *arg, int flags);
extern int get_userns_fd(unsigned long nsid, unsigned long hostid,
			 unsigned long range);
extern int get_userns_fd_from_idmap(struct list *idmap);
extern ssize_t read_nointr(int fd, void *buf, size_t count);
extern int wait_for_pid(pid_t pid);
extern ssize_t write_nointr(int fd, const void *buf, size_t count);

#endif /* __IDMAP_UTILS_H */
