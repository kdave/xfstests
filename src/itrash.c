/*
 * Bulkstat test case from Roger Willcocks <willcor@gmail.com>
 */
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

char buffer[32768];
int overwrote;

void die(char *func)
{
	perror(func);
	exit(1);
}

void nuke()
{
	int i;
	for (i = 2048; i < 32768-1; i++)
		if (buffer[i] == 'I' && buffer[i+1] == 'N') {
			buffer[i] = buffer[i+1] = 'X';
			overwrote = 1;
		}
}

int main(int argc, char* argv[])
{
	int f;
	loff_t offset;

	if (argc != 3) {
		printf("%s <device> <offset>\n", argv[0]);
		exit(1);
	}

	f = open(argv[1], O_RDWR);
	offset = atoll(argv[2]);

	if (f < 0) die("open");
	if (lseek(f, offset, SEEK_SET) < 0) die("lseek");
	if (read(f, buffer, 32768) != 32768) die("read");
	printf("Starting overwrite\n");
	nuke();
	if (lseek(f, offset, SEEK_SET) < 0) die("lseek");
	if (write(f, buffer, 32768) != 32768) die("write");
	if (!overwrote)
		printf("Did not overwrite any inodes\n");
	printf("Overwrite complete\n");
	close(f);
	return 0;
}
