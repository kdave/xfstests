/*
 *   aio-io-setup-with-nonwritable-context-pointer -
 *   Test what happens when a non-writable context pointer is passed to io_setup
 *   Copyright (C) 2007 Jeff Moyer
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
/*
 *  Author:  Jeff Moyer
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
	if (!addr) {
		perror("mmap");
		exit(1);
	}
	io_setup(1, addr /* un-writable pointer */);

	printf("%s: Success!\n", basename(argv[0]));
	return 0;
}
