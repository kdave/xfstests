// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 *
 * XFS had a memory corruption bug in its handling of the POSIX ACL attribute
 * names during a listxattr call.
 *
 * On IRIX, file ACLs were stored under the name "trusted.SGI_ACL_FILE",
 * whereas on Linux the name is "system.posix_acl_access".  In order to
 * maintain compatibility with old filesystems, XFS internally continues to
 * use the old SGI_ACL_FILE name on disk and remap the new name whenever it
 * sees it.
 *
 * In order to make this magic happen, XFS' listxattr implementation will emit
 * first the Linux name and then the on-disk name.  Unfortunately, it doesn't
 * correctly check the buffer length, so if the buffer is large enough to fit
 * the on-disk name but not large enough to fit the Linux name, we screw up
 * the buffer position accounting while trying to emit the Linux name and then
 * corrupt memory when we try to emit the on-disk name.
 *
 * If we trigger the bug, the program will print the garbled string
 * "rusted.SGI_ACL_FILE".  If the bug is fixed, the flistxattr call returns
 * ERANGE.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/xattr.h>

void die(const char *msg)
{
	perror(msg);
	exit(1);
}

struct entry {
	uint16_t	a;
	uint16_t	b;
	uint32_t	c;
};

struct myacl {
	uint32_t	d;
	struct entry	e[4];
};

int main(int argc, char *argv[])
{
	struct myacl acl = {
		.d = 2,
		.e = {
			{1, 0, 0},
			{4, 0, 0},
			{0x10, 0, 0},
			{0x20, 0, 0},
		},
	};
	char buf[64];
	ssize_t sz;
	int fd;
	int ret;

	if (argc > 1) {
		ret = chdir(argv[1]);
		if (ret)
			die(argv[1]);
	}

	fd = creat("file0", 0644);
	if (fd < 0)
		die("create");

	ret = fsetxattr(fd, "system.posix_acl_access", &acl, sizeof(acl), 0);
	if (ret)
		die("set posix acl");

	sz = flistxattr(fd, buf, 20);
	if (sz < 0)
		die("list attr");

	printf("%s\n", buf);

	return 0;

#if 0
	/* original syzkaller reproducer */

	syscall(__NR_mmap, 0x20000000, 0x1000000, 3, 0x32, -1, 0);

	memcpy((void*)0x20000180, "./file0", 8);
	syscall(__NR_creat, 0x20000180, 0);
	memcpy((void*)0x20000000, "./file0", 8);
	memcpy((void*)0x20000040, "system.posix_acl_access", 24);
	*(uint32_t*)0x20000680 = 2;
	*(uint16_t*)0x20000684 = 1;
	*(uint16_t*)0x20000686 = 0;
	*(uint32_t*)0x20000688 = 0;
	*(uint16_t*)0x2000068c = 4;
	*(uint16_t*)0x2000068e = 0;
	*(uint32_t*)0x20000690 = 0;
	*(uint16_t*)0x20000694 = 0x10;
	*(uint16_t*)0x20000696 = 0;
	*(uint32_t*)0x20000698 = 0;
	*(uint16_t*)0x2000069c = 0x20;
	*(uint16_t*)0x2000069e = 0;
	*(uint32_t*)0x200006a0 = 0;
	syscall(__NR_setxattr, 0x20000000, 0x20000040, 0x20000680, 0x24, 0);
	memcpy((void*)0x20000080, "./file0", 8);
	memcpy((void*)0x200000c0, "security.evm", 13);
	memcpy((void*)0x20000100, "\x03\x00\x00\x00\x57", 5);
	syscall(__NR_lsetxattr, 0x20000080, 0x200000c0, 0x20000100, 1, 1);
	memcpy((void*)0x20000300, "./file0", 8);
	syscall(__NR_listxattr, 0x20000300, 0x200002c0, 0x1e);
	return 0;
#endif
}
