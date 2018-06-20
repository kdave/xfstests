// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2007 Jeff Moyer
 */

/*
 *  Test what happens when a non-writable context pointer is passed to io_setup
 *
 *  Description: Pass a non-writable context pointer to io_setup to see if
 *  the kernel deals with it correctly.  In the past, the reference counting
 *  in this particular error path was off and this operation would cause an
 *  oops.
 *
 *  This is a destructive test.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <libgen.h>
#include <libaio.h>

int
main(int __attribute__((unused)) argc, char **argv)
{
	void *addr;

	addr = mmap(NULL, 4096, PROT_READ, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	io_setup(1, addr /* un-writable pointer */);

	printf("%s: Success!\n", basename(argv[0]));
	return 0;
}
