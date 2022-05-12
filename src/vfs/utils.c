// SPDX-License-Identifier: GPL-2.0
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <grp.h>
#include <linux/limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/fsuid.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/xattr.h>

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

static int get_userns_fd_cb(void *data)
{
	return 0;
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

static int write_id_mapping(idmap_type_t map_type, pid_t pid, const char *buf, size_t buf_size)
{
	int fd = -EBADF, setgroups_fd = -EBADF;
	int fret = -1;
	int ret;
	char path[STRLITERALLEN("/proc/") + INTTYPE_TO_STRLEN(pid_t) +
		  STRLITERALLEN("/setgroups") + 1];

	if (geteuid() != 0 && map_type == ID_TYPE_GID) {
		ret = snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
		if (ret < 0 || ret >= sizeof(path))
			goto out;

		setgroups_fd = open(path, O_WRONLY | O_CLOEXEC);
		if (setgroups_fd < 0 && errno != ENOENT) {
			syserror("Failed to open \"%s\"", path);
			goto out;
		}

		if (setgroups_fd >= 0) {
			ret = write_nointr(setgroups_fd, "deny\n", STRLITERALLEN("deny\n"));
			if (ret != STRLITERALLEN("deny\n")) {
				syserror("Failed to write \"deny\" to \"/proc/%d/setgroups\"", pid);
				goto out;
			}
		}
	}

	ret = snprintf(path, sizeof(path), "/proc/%d/%cid_map", pid, map_type == ID_TYPE_UID ? 'u' : 'g');
	if (ret < 0 || ret >= sizeof(path))
		goto out;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		syserror("Failed to open \"%s\"", path);
		goto out;
	}

	ret = write_nointr(fd, buf, buf_size);
	if (ret != buf_size) {
		syserror("Failed to write %cid mapping to \"%s\"",
			 map_type == ID_TYPE_UID ? 'u' : 'g', path);
		goto out;
	}

	fret = 0;
out:
	if (fd >= 0)
		close(fd);
	if (setgroups_fd >= 0)
		close(setgroups_fd);

	return fret;
}

static int map_ids_from_idmap(struct list *idmap, pid_t pid)
{
	int fill, left;
	char mapbuf[4096] = {};
	bool had_entry = false;
	idmap_type_t map_type, u_or_g;

	if (list_empty(idmap))
		return 0;

	for (map_type = ID_TYPE_UID, u_or_g = 'u';
	     map_type <= ID_TYPE_GID; map_type++, u_or_g = 'g') {
		char *pos = mapbuf;
		int ret;
		struct list *iterator;


		list_for_each(iterator, idmap) {
			struct id_map *map = iterator->elem;
			if (map->map_type != map_type)
				continue;

			had_entry = true;

			left = 4096 - (pos - mapbuf);
			fill = snprintf(pos, left, "%u %u %u\n", map->nsid, map->hostid, map->range);
			/*
			 * The kernel only takes <= 4k for writes to
			 * /proc/<pid>/{g,u}id_map
			 */
			if (fill <= 0 || fill >= left)
				return syserror_set(-E2BIG, "Too many %cid mappings defined", u_or_g);

			pos += fill;
		}
		if (!had_entry)
			continue;

		ret = write_id_mapping(map_type, pid, mapbuf, pos - mapbuf);
		if (ret < 0)
			return syserror("Failed to write mapping: %s", mapbuf);

		memset(mapbuf, 0, sizeof(mapbuf));
	}

	return 0;
}

#ifdef DEBUG_TRACE
static void __print_idmaps(pid_t pid, bool gid)
{
	char path_mapping[STRLITERALLEN("/proc/") + INTTYPE_TO_STRLEN(pid_t) +
			  STRLITERALLEN("/_id_map") + 1];
	char *line = NULL;
	size_t len = 0;
	int ret;
	FILE *f;

	ret = snprintf(path_mapping, sizeof(path_mapping), "/proc/%d/%cid_map",
		       pid, gid ? 'g' : 'u');
	if (ret < 0 || (size_t)ret >= sizeof(path_mapping))
		return;

	f = fopen(path_mapping, "r");
	if (!f)
		return;

	while ((ret = getline(&line, &len, f)) > 0)
		fprintf(stderr, "%s", line);

	fclose(f);
	free(line);
}

static void print_idmaps(pid_t pid)
{
	__print_idmaps(pid, false);
	__print_idmaps(pid, true);
}
#else
static void print_idmaps(pid_t pid)
{
}
#endif

int get_userns_fd_from_idmap(struct list *idmap)
{
	int ret;
	pid_t pid;
	char path_ns[STRLITERALLEN("/proc/") + INTTYPE_TO_STRLEN(pid_t) +
		     STRLITERALLEN("/ns/user") + 1];

	pid = do_clone(get_userns_fd_cb, NULL, CLONE_NEWUSER | CLONE_NEWNS);
	if (pid < 0)
		return -errno;

	ret = map_ids_from_idmap(idmap, pid);
	if (ret < 0)
		return ret;

	ret = snprintf(path_ns, sizeof(path_ns), "/proc/%d/ns/user", pid);
	if (ret < 0 || (size_t)ret >= sizeof(path_ns)) {
		ret = -EIO;
	} else {
		ret = open(path_ns, O_RDONLY | O_CLOEXEC | O_NOCTTY);
		print_idmaps(pid);
	}

	(void)kill(pid, SIGKILL);
	(void)wait_for_pid(pid);
	return ret;
}

int get_userns_fd(unsigned long nsid, unsigned long hostid, unsigned long range)
{
	struct list head, uid_mapl, gid_mapl;
	struct id_map uid_map = {
		.map_type	= ID_TYPE_UID,
		.nsid		= nsid,
		.hostid		= hostid,
		.range		= range,
	};
	struct id_map gid_map = {
		.map_type	= ID_TYPE_GID,
		.nsid		= nsid,
		.hostid		= hostid,
		.range		= range,
	};

	list_init(&head);
	uid_mapl.elem = &uid_map;
	gid_mapl.elem = &gid_map;
	list_add_tail(&head, &uid_mapl);
	list_add_tail(&head, &gid_mapl);

	return get_userns_fd_from_idmap(&head);
}

bool switch_ids(uid_t uid, gid_t gid)
{
	if (setgroups(0, NULL))
		return syserror("failure: setgroups");

	if (setresgid(gid, gid, gid))
		return syserror("failure: setresgid");

	if (setresuid(uid, uid, uid))
		return syserror("failure: setresuid");

	return true;
}

static int userns_fd_cb(void *data)
{
	struct userns_hierarchy *h = data;
	char c;
	int ret;

	ret = read_nointr(h->fd_event, &c, 1);
	if (ret < 0)
		return syserror("failure: read from socketpair");

	/* Only switch ids if someone actually wrote a mapping for us. */
	if (c == '1') {
		if (!switch_ids(0, 0))
			return syserror("failure: switch ids to 0");

		/* Ensure we can access proc files from processes we can ptrace. */
		ret = prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
		if (ret < 0)
			return syserror("failure: make dumpable");
	}

	ret = write_nointr(h->fd_event, "1", 1);
	if (ret < 0)
		return syserror("failure: write to socketpair");

	ret = create_userns_hierarchy(++h);
	if (ret < 0)
		return syserror("failure: userns level %d", h->level);

	return 0;
}

int create_userns_hierarchy(struct userns_hierarchy *h)
{
	int fret = -1;
	char c;
	int fd_socket[2];
	int fd_userns = -EBADF, ret = -1;
	ssize_t bytes;
	pid_t pid;
	char path[256];

	if (h->level == MAX_USERNS_LEVEL)
		return 0;

	ret = socketpair(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_socket);
	if (ret < 0)
		return syserror("failure: create socketpair");

	/* Note the CLONE_FILES | CLONE_VM when mucking with fds and memory. */
	h->fd_event = fd_socket[1];
	pid = do_clone(userns_fd_cb, h, CLONE_NEWUSER | CLONE_FILES | CLONE_VM);
	if (pid < 0) {
		syserror("failure: userns level %d", h->level);
		goto out_close;
	}

	ret = map_ids_from_idmap(&h->id_map, pid);
	if (ret < 0) {
		kill(pid, SIGKILL);
		syserror("failure: writing id mapping for userns level %d for %d", h->level, pid);
		goto out_wait;
	}

	if (!list_empty(&h->id_map))
		bytes = write_nointr(fd_socket[0], "1", 1); /* Inform the child we wrote a mapping. */
	else
		bytes = write_nointr(fd_socket[0], "0", 1); /* Inform the child we didn't write a mapping. */
	if (bytes < 0) {
		kill(pid, SIGKILL);
		syserror("failure: write to socketpair");
		goto out_wait;
	}

	/* Wait for child to set*id() and become dumpable. */
	bytes = read_nointr(fd_socket[0], &c, 1);
	if (bytes < 0) {
		kill(pid, SIGKILL);
		syserror("failure: read from socketpair");
		goto out_wait;
	}

	snprintf(path, sizeof(path), "/proc/%d/ns/user", pid);
	fd_userns = open(path, O_RDONLY | O_CLOEXEC);
	if (fd_userns < 0) {
		kill(pid, SIGKILL);
		syserror("failure: open userns level %d for %d", h->level, pid);
		goto out_wait;
	}

	fret = 0;

out_wait:
	if (!wait_for_pid(pid) && !fret) {
		h->fd_userns = fd_userns;
		fd_userns = -EBADF;
	}

out_close:
	if (fd_userns >= 0)
		close(fd_userns);
	close(fd_socket[0]);
	close(fd_socket[1]);
	return fret;
}

int add_map_entry(struct list *head,
		  __u32 id_host,
		  __u32 id_ns,
		  __u32 range,
		  idmap_type_t map_type)
{
	struct list *new_list = NULL;
	struct id_map *newmap = NULL;

	newmap = malloc(sizeof(*newmap));
	if (!newmap)
		return -ENOMEM;

	new_list = malloc(sizeof(struct list));
	if (!new_list) {
		free(newmap);
		return -ENOMEM;
	}

	*newmap = (struct id_map){
		.hostid		= id_host,
		.nsid		= id_ns,
		.range		= range,
		.map_type	= map_type,
	};

	new_list->elem = newmap;
	list_add_tail(head, new_list);
	return 0;
}

/* __expected_uid_gid - check whether file is owned by the provided uid and gid */
bool __expected_uid_gid(int dfd, const char *path, int flags,
			uid_t expected_uid, gid_t expected_gid, bool log)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return log_errno(false, "failure: fstatat");

	if (log && st.st_uid != expected_uid)
		log_stderr("failure: uid(%d) != expected_uid(%d)", st.st_uid, expected_uid);

	if (log && st.st_gid != expected_gid)
		log_stderr("failure: gid(%d) != expected_gid(%d)", st.st_gid, expected_gid);

	errno = 0; /* Don't report misleading errno. */
	return st.st_uid == expected_uid && st.st_gid == expected_gid;
}

/* caps_down - lower all effective caps */
int caps_down(void)
{
	bool fret = false;
#ifdef HAVE_SYS_CAPABILITY_H
	cap_t caps = NULL;
	int ret = -1;

	caps = cap_get_proc();
	if (!caps)
		goto out;

	ret = cap_clear_flag(caps, CAP_EFFECTIVE);
	if (ret)
		goto out;

	ret = cap_set_proc(caps);
	if (ret)
		goto out;

	fret = true;

out:
	cap_free(caps);
#endif
	return fret;
}

/* caps_down_fsetid - lower CAP_FSETID effective cap */
int caps_down_fsetid(void)
{
	bool fret = false;
#ifdef HAVE_SYS_CAPABILITY_H
	cap_t caps = NULL;
	cap_value_t cap = CAP_FSETID;
	int ret = -1;

	caps = cap_get_proc();
	if (!caps)
		goto out;

	ret = cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, 0);
	if (ret)
		goto out;

	ret = cap_set_proc(caps);
	if (ret)
		goto out;

	fret = true;

out:
	cap_free(caps);
#endif
	return fret;
}

#ifdef HAVE_LIBURING_H
int io_uring_openat_with_creds(struct io_uring *ring, int dfd, const char *path,
			       int cred_id, bool with_link, int *ret_cqe)
{
	struct io_uring_cqe *cqe;
	struct io_uring_sqe *sqe;
	int ret, i, to_submit = 1;

	if (with_link) {
		sqe = io_uring_get_sqe(ring);
		if (!sqe)
			return log_error_errno(-EINVAL, EINVAL, "failure: io_uring_sqe");
		io_uring_prep_nop(sqe);
		sqe->flags |= IOSQE_IO_LINK;
		sqe->user_data = 1;
		to_submit++;
	}

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		return log_error_errno(-EINVAL, EINVAL, "failure: io_uring_sqe");
	io_uring_prep_openat(sqe, dfd, path, O_RDONLY | O_CLOEXEC, 0);
	sqe->user_data = 2;

	if (cred_id != -1)
		sqe->personality = cred_id;

	ret = io_uring_submit(ring);
	if (ret != to_submit) {
		log_stderr("failure: io_uring_submit");
		goto out;
	}

	for (i = 0; i < to_submit; i++) {
		ret = io_uring_wait_cqe(ring, &cqe);
		if (ret < 0) {
			log_stderr("failure: io_uring_wait_cqe");
			goto out;
		}

		ret = cqe->res;
		/*
		 * Make sure caller can identify that this is a proper io_uring
		 * failure and not some earlier error.
		 */
		if (ret_cqe)
			*ret_cqe = ret;
		io_uring_cqe_seen(ring, cqe);
	}
	log_debug("Ran test");
out:
	return ret;
}
#endif /* HAVE_LIBURING_H */

/* caps_up - raise all permitted caps */
int caps_up(void)
{
	bool fret = false;
#ifdef HAVE_SYS_CAPABILITY_H
	cap_t caps = NULL;
	cap_value_t cap;
	int ret = -1;

	caps = cap_get_proc();
	if (!caps)
		goto out;

	for (cap = 0; cap <= CAP_LAST_CAP; cap++) {
		cap_flag_value_t flag;

		ret = cap_get_flag(caps, cap, CAP_PERMITTED, &flag);
		if (ret) {
			if (errno == EINVAL)
				break;
			else
				goto out;
		}

		ret = cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, flag);
		if (ret)
			goto out;
	}

	ret = cap_set_proc(caps);
	if (ret)
		goto out;

	fret = true;
out:
	cap_free(caps);
#endif
	return fret;
}

/* chown_r - recursively change ownership of all files */
int chown_r(int fd, const char *path, uid_t uid, gid_t gid)
{
	int dfd, ret;
	DIR *dir;
	struct dirent *direntp;

	dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY);
	if (dfd < 0)
		return -1;

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		return -1;
	}

	while ((direntp = readdir(dir))) {
		struct stat st;

		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		if (S_ISDIR(st.st_mode))
			ret = chown_r(dfd, direntp->d_name, uid, gid);
		else
			ret = fchownat(dfd, direntp->d_name, uid, gid, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	ret = fchownat(fd, path, uid, gid, AT_SYMLINK_NOFOLLOW);
	closedir(dir);
	return ret;
}

/* expected_dummy_vfs_caps_uid - check vfs caps are stored with the provided uid */
bool expected_dummy_vfs_caps_uid(int fd, uid_t expected_uid)
{
#define __cap_raised_permitted(x, ns_cap_data)                                 \
	((ns_cap_data.data[(x) >> 5].permitted) & (1 << ((x)&31)))
	struct vfs_ns_cap_data ns_xattr = {};
	ssize_t ret;

	ret = fgetxattr(fd, "security.capability", &ns_xattr, sizeof(ns_xattr));
	if (ret < 0 || ret == 0)
		return false;

	if (ns_xattr.magic_etc & VFS_CAP_REVISION_3) {

		if (le32_to_cpu(ns_xattr.rootid) != expected_uid) {
			errno = EINVAL;
			log_stderr("failure: rootid(%d) != expected_rootid(%d)", le32_to_cpu(ns_xattr.rootid), expected_uid);
		}

		return (le32_to_cpu(ns_xattr.rootid) == expected_uid) &&
		       (__cap_raised_permitted(CAP_NET_RAW, ns_xattr) > 0);
	} else {
		log_stderr("failure: fscaps version");
	}

	return false;
}

/* set_dummy_vfs_caps - set dummy vfs caps for the provided uid */
int set_dummy_vfs_caps(int fd, int flags, int rootuid)
{
#define __raise_cap_permitted(x, ns_cap_data)                                  \
	ns_cap_data.data[(x) >> 5].permitted |= (1 << ((x)&31))

	struct vfs_ns_cap_data ns_xattr;

	memset(&ns_xattr, 0, sizeof(ns_xattr));
	__raise_cap_permitted(CAP_NET_RAW, ns_xattr);
	ns_xattr.magic_etc |= VFS_CAP_REVISION_3 | VFS_CAP_FLAGS_EFFECTIVE;
	ns_xattr.rootid = cpu_to_le32(rootuid);

	return fsetxattr(fd, "security.capability",
			 &ns_xattr, sizeof(ns_xattr), flags);
}

bool protected_symlinks_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		int fd;
		ssize_t ret;
		char buf[256];

		enabled = 0;

		fd = open("/proc/sys/fs/protected_symlinks", O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return false;

		ret = read(fd, buf, sizeof(buf));
		close(fd);
		if (ret < 0)
			return false;

		if (atoi(buf) >= 1)
			enabled = 1;
        }

	return enabled == 1;
}

static bool is_xfs(const char *fstype)
{
	static int enabled = -1;

	if (enabled == -1)
		enabled = !strcmp(fstype, "xfs");

	return enabled;
}

bool xfs_irix_sgid_inherit_enabled(const char *fstype)
{
	static int enabled = -1;

	if (enabled == -1) {
		int fd;
		ssize_t ret;
		char buf[256];

		enabled = 0;

		if (is_xfs(fstype)) {
			fd = open("/proc/sys/fs/xfs/irix_sgid_inherit", O_RDONLY | O_CLOEXEC);
			if (fd < 0)
				return false;

			ret = read(fd, buf, sizeof(buf));
			close(fd);
			if (ret < 0)
				return false;

			if (atoi(buf) >= 1)
				enabled = 1;
		}
        }

	return enabled == 1;
}

bool expected_file_size(int dfd, const char *path, int flags, off_t expected_size)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return log_errno(false, "failure: fstatat");

	if (st.st_size != expected_size)
		return log_errno(false, "failure: st_size(%zu) != expected_size(%zu)",
				 (size_t)st.st_size, (size_t)expected_size);

	return true;
}

/* is_setid - check whether file is S_ISUID and S_ISGID */
bool is_setid(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISUID) || (st.st_mode & S_ISGID);
}

/* is_setgid - check whether file or directory is S_ISGID */
bool is_setgid(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISGID);
}

/* is_sticky - check whether file is S_ISVTX */
bool is_sticky(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISVTX) > 0;
}

bool switch_resids(uid_t uid, gid_t gid)
{
	if (setresgid(gid, gid, gid))
		return log_errno(false, "failure: setregid");

	if (setresuid(uid, uid, uid))
		return log_errno(false, "failure: setresuid");

	if (setfsgid(-1) != gid)
		return log_errno(false, "failure: setfsgid(-1)");

	if (setfsuid(-1) != uid)
		return log_errno(false, "failure: setfsuid(-1)");

	return true;
}

/* rm_r - recursively remove all files */
int rm_r(int fd, const char *path)
{
	int dfd, ret;
	DIR *dir;
	struct dirent *direntp;

	if (!path || strcmp(path, "") == 0)
		return -1;

	dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY);
	if (dfd < 0)
		return -1;

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		return -1;
	}

	while ((direntp = readdir(dir))) {
		struct stat st;

		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		if (S_ISDIR(st.st_mode))
			ret = rm_r(dfd, direntp->d_name);
		else
			ret = unlinkat(dfd, direntp->d_name, 0);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	ret = unlinkat(fd, path, AT_REMOVEDIR);
	closedir(dir);
	return ret;
}

/* fd_to_fd - transfer data from one fd to another */
int fd_to_fd(int from, int to)
{
	for (;;) {
		uint8_t buf[PATH_MAX];
		uint8_t *p = buf;
		ssize_t bytes_to_write;
		ssize_t bytes_read;

		bytes_read = read_nointr(from, buf, sizeof buf);
		if (bytes_read < 0)
			return -1;
		if (bytes_read == 0)
			break;

		bytes_to_write = (size_t)bytes_read;
		do {
			ssize_t bytes_written;

			bytes_written = write_nointr(to, p, bytes_to_write);
			if (bytes_written < 0)
				return -1;

			bytes_to_write -= bytes_written;
			p += bytes_written;
		} while (bytes_to_write > 0);
	}

	return 0;
}

bool openat_tmpfile_supported(int dirfd)
{
	int fd = -1;

	fd = openat(dirfd, ".", O_TMPFILE | O_RDWR, S_IXGRP | S_ISGID);
	if (fd == -1) {
		if (errno == ENOTSUP)
			return false;
		else
			return log_errno(false, "failure: create");
	}

	if (close(fd))
		log_stderr("failure: close");

	return true;
}

/*
 * There'll be scenarios where you'll want to see the attributes associated with
 * a directory tree during debugging or just to make sure things look correct.
 * Simply uncomment and place the print_r() helper where you need it.
 */
#ifdef DEBUG_TRACE
static int fd_cloexec(int fd, bool cloexec)
{
	int oflags, nflags;

	oflags = fcntl(fd, F_GETFD, 0);
	if (oflags < 0)
		return -errno;

	if (cloexec)
		nflags = oflags | FD_CLOEXEC;
	else
		nflags = oflags & ~FD_CLOEXEC;

	if (nflags == oflags)
		return 0;

	if (fcntl(fd, F_SETFD, nflags) < 0)
		return -errno;

	return 0;
}

static inline int dup_cloexec(int fd)
{
	int fd_dup;

	fd_dup = dup(fd);
	if (fd_dup < 0)
		return -errno;

	if (fd_cloexec(fd_dup, true)) {
		close(fd_dup);
		return -errno;
	}

	return fd_dup;
}

int print_r(int fd, const char *path)
{
	int ret = 0;
	int dfd, dfd_dup;
	DIR *dir;
	struct dirent *direntp;
	struct stat st;

	if (!path || *path == '\0') {
		char buf[sizeof("/proc/self/fd/") + 30];

		ret = snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
		if (ret < 0 || (size_t)ret >= sizeof(buf))
			return -1;

		/*
		 * O_PATH file descriptors can't be used so we need to re-open
		 * just in case.
		 */
		dfd = openat(-EBADF, buf, O_CLOEXEC | O_DIRECTORY, 0);
	} else {
		dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY, 0);
	}
	if (dfd < 0)
		return -1;

	/*
	 * When fdopendir() below succeeds it assumes ownership of the fd so we
	 * to make sure we always have an fd that fdopendir() can own which is
	 * why we dup() in the case where the caller wants us to operate on the
	 * fd directly.
	 */
	dfd_dup = dup_cloexec(dfd);
	if (dfd_dup < 0) {
		close(dfd);
		return -1;
	}

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		close(dfd_dup);
		return -1;
	}
	/* Transfer ownership to fdopendir(). */
	dfd = -EBADF;

	while ((direntp = readdir(dir))) {
		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd_dup, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		ret = 0;
		if (S_ISDIR(st.st_mode))
			ret = print_r(dfd_dup, direntp->d_name);
		else
			fprintf(stderr, "mode(%o):uid(%d):gid(%d) -> %d/%s\n",
				(st.st_mode & ~S_IFMT), st.st_uid, st.st_gid,
				dfd_dup, direntp->d_name);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	if (!path || *path == '\0')
		ret = fstatat(fd, "", &st,
			      AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW |
			      AT_EMPTY_PATH);
	else
		ret = fstatat(fd, path, &st,
			      AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
	if (!ret)
		fprintf(stderr, "mode(%o):uid(%d):gid(%d) -> %s\n",
			(st.st_mode & ~S_IFMT), st.st_uid, st.st_gid,
			(path && *path) ? path : "(null)");

	close(dfd_dup);
	closedir(dir);

	return ret;
}
#endif
