#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	printf("%u\n", getpagesize());
	return 0;
}
