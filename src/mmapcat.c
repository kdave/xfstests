/* mmapcat.c - derived from source by misiek@pld.ORG.PL */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	int fd;
	char *ptr, *ptr2;
	struct stat st;

	fd=open(argv[1],O_RDONLY);
	if(fd<0) {
		perror(argv[1]);
		exit(1);
	}
	fstat(fd,&st);
	if(st.st_size%4096==0) {
		fprintf(stderr,"bad file size!\n");
		exit(1);
	}

	ptr2 = ptr = mmap(NULL,st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
	while (*ptr!=0) ptr++;
	write(1,ptr2,ptr - ptr2);
	exit(0);
}

