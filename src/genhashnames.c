/*
 * Creates a bunch of files in the specified directory with the same
 * hashvalue on-disk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#define rol32(x,y) (((x) << (y)) | ((x) >> (32 - (y))))

/*
 * Implement a simple hash on a character string.
 * Rotate the hash value by 7 bits, then XOR each character in.
 * This is implemented with some source-level loop unrolling.
 */
static uint32_t xfs_da_hashname(const char *name, int namelen)
{
	uint32_t	hash;

	/*
	 * Do four characters at a time as long as we can.
	 */
	for (hash = 0; namelen >= 4; namelen -= 4, name += 4)
		hash = (name[0] << 21) ^ (name[1] << 14) ^ (name[2] << 7) ^
		       (name[3] << 0) ^ rol32(hash, 7 * 4);

	/*
	 * Now do the rest of the characters.
	 */
	switch (namelen) {
	case 3:
		return (name[0] << 14) ^ (name[1] << 7) ^ (name[2] << 0) ^
		       rol32(hash, 7 * 3);
	case 2:
		return (name[0] << 7) ^ (name[1] << 0) ^ rol32(hash, 7 * 2);
	case 1:
		return (name[0] << 0) ^ rol32(hash, 7 * 1);
	default: /* case 0: */
		return hash;
	}
}

static int is_invalid_char(char c)
{
	return (c <= ' ' || c > '~' || c == '/' || (c >= 'A' && c <= 'Z'));
}

static char random_char(void)
{
	char	c;

	/* get a character of "0"-"9" or "a"-"z" */

	c = lrand48() % 36;

	return (c >= 10) ? c + 87 : c + 48;
}

static int generate_names(const char *path, long amount, uint32_t desired_hash)
{
	char		**names;
	char		fullpath[PATH_MAX];
	char		*filename;
	long		count;
	long		i;
	uint32_t	base_hash;
	uint32_t	hash;

	names = malloc(amount * sizeof(char *));
	if (!names) {
		fprintf(stderr, "genhashnames: malloc(%lu) failed!\n",
					amount * sizeof(char *));
		return 1;
	}

	strcpy(fullpath, path);
	filename = fullpath + strlen(fullpath);
	if (filename[-1] != '/')
		*filename++ = '/';

	for (count = 0; count < amount; count++) {
		for (;;) {
			base_hash = 0;
			for (i = 0; i < 6; i++) {
				filename[i] = random_char();
				base_hash = filename[i] ^ rol32(base_hash, 7);
			}
			while (i < 200) {
				filename[i] = random_char();
				base_hash = filename[i] ^ rol32(base_hash, 7);
				i++;
				hash = rol32(base_hash, 3) ^ desired_hash;

				filename[i] = (hash >> 28) |
							(random_char() & 0xf0);
				if (is_invalid_char(filename[i]))
					continue;

				filename[i + 1] = (hash >> 21) & 0x7f;
				if (is_invalid_char(filename[i + 1]))
					continue;
				filename[i + 2] = (hash >> 14) & 0x7f;
				if (is_invalid_char(filename[i + 2]))
					continue;
				filename[i + 3] = (hash >> 7) & 0x7f;
				if (is_invalid_char(filename[i + 3]))
					continue;
				filename[i + 4] = (hash ^ (filename[i] >> 4))
									& 0x7f;
				if (is_invalid_char(filename[i + 4]))
					continue;
				break;
			}
			if (i < NAME_MAX)
				break;
		}
		filename[i + 5] = '\0';
		if (xfs_da_hashname(filename, i + 5) != desired_hash) {
			fprintf(stderr, "genhashnames: Hash mismatch!\n");
			return 1;
		}

		for (i = 0; i < count; i++) {
			if (strcmp(fullpath, names[i]) == 0)
				break;
		}
		if (i == count) {
			names[count] = strdup(fullpath);
			puts(fullpath);
		} else
			count--;
	}
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "Usage: genhashnames <directory> <amount> "
			"[seed] [hashvalue]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	long		seed;
	uint32_t	desired_hash;

	if (argc < 3 || argc > 5)
		usage();

	if (argc >= 4)
		seed = strtol(argv[3], NULL, 0);
	else {
		struct timeval	tv;
		gettimeofday(&tv, NULL);
		seed = tv.tv_usec / 1000 + (tv.tv_sec % 1000) * 1000;
	}
	srand48(seed);

	/*
	 * always generate hash from random so if hash is specified, random
	 * sequence is the same as a randomly generated hash of the same value.
	 */
	desired_hash = (uint32_t)mrand48();
	if (argc >= 5)
		desired_hash = (uint32_t)strtoul(argv[4], NULL, 0);

	fprintf(stderr, "seed = %ld, hash = 0x%08x\n", seed, desired_hash);

	return generate_names(argv[1], strtol(argv[2], NULL, 0), desired_hash);
}
