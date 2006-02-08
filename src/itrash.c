/*
 * Bulkstat test case from Roger Willcocks <willcor@gmail.com>
 */
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

char buffer[32768];

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
			printf("Overwrote IN @offset %d\n", i);
		}
}

int main(int argc, char* argv[])
{
	int f = open(argv[1], O_RDWR);
	if (f < 0) die("open");
	if (lseek(f, 32768, SEEK_SET) < 0) die("lseek");
	if (read(f, buffer, 32768) != 32768) die("read");
	printf("Starting overwrite\n");
	nuke();
	if (lseek(f, 32768, SEEK_SET) < 0) die("lseek");
	if (write(f, buffer, 32768) != 32768) die("write");
	printf("Overwrite complete\n");
	close(f);
	return 0;
}
