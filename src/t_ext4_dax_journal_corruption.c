#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PAGE(a) ((a)*0x1000)
#define STRLEN 256

void err_exit(char *op)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errno));
	exit(1);
}

void chattr_cmd(char *chattr, char *cmd, char *file)
{
	int ret;
	char command[STRLEN];

	ret = snprintf(command, STRLEN, "%s %s %s 2>/dev/null", chattr, cmd, file);
	if (ret < 0)
		err_exit("snprintf");

	ret = system(command);
	if (ret) /* Success - the kernel fix is to have this chattr fail */
		exit(77);
}

int main(int argc, char *argv[])
{
	int fd, err, len = PAGE(1);
	char *data, *dax_data, *chattr, *file;
	char string[STRLEN];

	if (argc < 3) {
		printf("Usage: %s <chattr program> <file>\n", basename(argv[0]));
		exit(0);
	}

	chattr = argv[1];
	file = argv[2];

	srand(time(NULL));
	snprintf(string, STRLEN, "random number %d\n", rand());

	fd = open(file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd < 0)
		err_exit("fd");

	/* begin with journaling off and DAX on */
	chattr_cmd(chattr, "-j", file);

	ftruncate(fd, 0);
	fallocate(fd, 0, 0, len);

	dax_data = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (!dax_data)
		err_exit("mmap dax_data");

	/*
	 * This turns on journaling.  It also has the side-effect that it
	 * turns off DAX for the given inode since journaling and DAX aren't
	 * allowed to be on at the same time.  This happens in
	 * ext4_change_inode_journal_flag() in kernel v4.14 and before.
	 *
	 * Note that this turns off the runtime DAX flag (S_DAX) in the
	 * in-memory inode, and has nothing to do with per-inode on-media DAX
	 * inode flags.
	 */
	chattr_cmd(chattr, "+j", file);

	data = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (!data)
		err_exit("mmap data");

	/*
	 * Write the data using the non-DAX mapping, and try and read it back
	 * using the DAX mapping.
	 */
	strcpy(data, string);
	if (strcmp(dax_data, string) != 0)
		printf("Data miscompare\n");

	err = munmap(data, len);
	if (err < 0)
		err_exit("munmap data");

	err = munmap(dax_data, len);
	if (err < 0)
		err_exit("munmap dax_data");

	err = close(fd);
	if (err < 0)
		err_exit("close");
	return 0;
}
