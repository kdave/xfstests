/* mkswap(8) without any sanity checks */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

struct swap_header {
	char		bootbits[1024];
	uint32_t	version;
	uint32_t	last_page;
	uint32_t	nr_badpages;
	unsigned char	sws_uuid[16];
	unsigned char	sws_volume[16];
	uint32_t	padding[117];
	uint32_t	badpages[1];
};

int main(int argc, char **argv)
{
	struct swap_header *hdr;
	FILE *file;
	struct stat st;
	long page_size;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "usage: %s PATH\n", argv[0]);
		return EXIT_FAILURE;
	}

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size == -1) {
		perror("sysconf");
		return EXIT_FAILURE;
	}

	hdr = calloc(1, page_size);
	if (!hdr) {
		perror("calloc");
		return EXIT_FAILURE;
	}

	file = fopen(argv[1], "r+");
	if (!file) {
		perror("fopen");
		free(hdr);
		return EXIT_FAILURE;
	}

	ret = fstat(fileno(file), &st);
	if (ret) {
		perror("fstat");
		free(hdr);
		fclose(file);
		return EXIT_FAILURE;
	}

	hdr->version = 1;
	hdr->last_page = st.st_size / page_size - 1;
	memset(&hdr->sws_uuid, 0x99, sizeof(hdr->sws_uuid));
	memcpy((char *)hdr + page_size - 10, "SWAPSPACE2", 10);

	if (fwrite(hdr, page_size, 1, file) != 1) {
		perror("fwrite");
		free(hdr);
		fclose(file);
		return EXIT_FAILURE;
	}

	if (fclose(file) == EOF) {
		perror("fwrite");
		free(hdr);
		return EXIT_FAILURE;
	}

	free(hdr);

	return EXIT_SUCCESS;
}
