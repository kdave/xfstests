#define _LARGEFILE64_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

/* Our own declaration taken from the kernel since glibc does not have it... */
struct linux_dirent64 {
	uint64_t	d_ino;
	int64_t		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[];
};

#define MIN_NAME_LEN 8
#define MAX_NAME_LEN 70

static DIR *dir;
static int dfd;
static int ignore_error;

struct dir_ops {
	loff_t (*getpos)(void);
	void (*setpos)(loff_t pos);
	void (*getentry)(struct dirent *entry);
};

static off64_t libc_getpos(void)
{
	return telldir(dir);
}

static void libc_setpos(off64_t pos)
{
	seekdir(dir, pos);
}

static void libc_getentry(struct dirent *entry)
{
	struct dirent *ret;

	errno = 0;
	ret = readdir(dir);
	if (!ret) {
		if (errno == 0) {
			fprintf(stderr, "Unexpected EOF while reading dir.\n");
			exit(1);
		}
		if (ignore_error)
			return;
		perror("readdir");
		exit(1);
	}
	memcpy(entry, ret, sizeof(struct dirent));
}

static off64_t kernel_getpos(void)
{
	return lseek64(dfd, 0, SEEK_CUR);
}

static void kernel_setpos(off64_t pos)
{
	lseek64(dfd, pos, SEEK_SET);
}

static void kernel_getentry(struct dirent *entry)
{
	char dirbuf[NAME_MAX + 1 + sizeof(struct linux_dirent64)];
	struct linux_dirent64 *lentry = (struct linux_dirent64 *)dirbuf;
	int ret;

	ret = syscall(SYS_getdents64, dfd, lentry, sizeof(dirbuf));
	if (ret < 0) {
		if (ignore_error)
			return;
		perror("getdents64");
		exit(1);
	}
	if (ret == 0) {
		fprintf(stderr, "Unexpected EOF while reading dir.\n");
		exit(1);
	}
	entry->d_ino = lentry->d_ino;
	entry->d_off = lentry->d_off;
	entry->d_reclen = lentry->d_reclen;
	entry->d_type = lentry->d_type;
	strcpy(entry->d_name, lentry->d_name);
}

struct dir_ops libc_ops = {
	.getpos = libc_getpos,
	.setpos = libc_setpos,
	.getentry = libc_getentry,
};

struct dir_ops kernel_ops = {
	.getpos = kernel_getpos,
	.setpos = kernel_setpos,
	.getentry = kernel_getentry,
};

static void create_dir(char *dir, int count)
{
	int i, j, len;
	char namebuf[MAX_NAME_LEN];
	int dfd, fd;

	dfd = open(dir, O_RDONLY | O_DIRECTORY);
	if (dfd < 0) {
		perror("Cannot open dir");
		exit(1);
	}
	for (i = 0; i < count; i++) {
		len = random() % (MAX_NAME_LEN - MIN_NAME_LEN) + MIN_NAME_LEN;
		for (j = 0; j < len; j++)
			namebuf[j] = random() % 26 + 'a';
		namebuf[len] = 0;

		fd = openat(dfd, namebuf, O_RDWR | O_CREAT | O_EXCL, 0644);
		if (fd < 0) {
			if (errno == EEXIST) {
				/* Try again */
				i--;
				continue;
			}
			perror("File creation failed");
			exit(1);
		}
		close(fd);
	}
	close(dfd);
}

static void test(int count, struct dir_ops *ops)
{
	struct dirent *dbuf;
	struct dirent entry;
	loff_t *pbuf;
	loff_t dpos, maxpos = 0;
	int i, pos;

	dbuf = calloc(count, sizeof(struct dirent));
	pbuf = calloc(count, sizeof(loff_t));
	if (!dbuf || !pbuf) {
		fprintf(stderr, "Out of memory for buffers.\n");
		exit(1);
	}

	for (i = 0; i < count; i++) {
		pbuf[i] = ops->getpos();
		if (pbuf[i] > maxpos)
			maxpos = pbuf[i];
		ops->getentry(dbuf + i);
		ops->setpos(dbuf[i].d_off);
	}

	for (i = 0; i < count; i++) {
		pos = random() % count;
		ops->setpos(pbuf[pos]);
		ops->getentry(&entry);
		if (dbuf[pos].d_ino != entry.d_ino ||
		    dbuf[pos].d_type != entry.d_type ||
		    strcmp(dbuf[pos].d_name, entry.d_name)) {
			fprintf(stderr,
				"Mismatch in dir entry %u at pos %llu\n", pos,
				(unsigned long long)pbuf[pos]);
			exit(1);
		}
	}
	puts("Reading valid entries passed.");

	ignore_error = 1;
	for (i = 0; i < count; i++) {
		dpos = random() % maxpos;
		ops->setpos(dpos);
		/*
		 * We don't care about the result but the kernel should not
		 * crash.
		 */
		ops->getentry(&entry);
	}
	ignore_error = 0;

	puts("Reading random positions passed.");
	free(dbuf);
	free(pbuf);
}

int main(int argc, char *argv[])
{
	int count;
	unsigned long seed;

	if (argc != 4) {
		fprintf(stderr, "Usage: t_seekdir_3 <dir> <count> <seed>\n");
		return 1;
	}

	count = atoi(argv[2]);
	seed = atol(argv[3]);

	srandom(seed);

	create_dir(argv[1], count);

	puts("Testing readdir...");
	dir = opendir(argv[1]);
	if (!dir) {
		perror("Cannot open dir");
		exit(1);
	}
	test(count, &libc_ops);
	closedir(dir);
	dir = NULL;

	puts("Testing getdents...");
	dfd = open(argv[1], O_DIRECTORY | O_RDONLY);
	if (dfd < 0) {
		perror("Cannot open dir");
		exit(1);
	}
	test(count, &kernel_ops);
	close(dfd);
	dfd = 0;
	fprintf(stderr, "All tests passed\n");

	return 0;
}
