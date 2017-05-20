/* Perform various tests on stat and statx output
 *
 * Copyright (C) 2017 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "statx.h"

static bool failed = false;
static bool is_verbose = 0;
static const char *prog;
static const char *testfile;

/* Reference data */
static struct statx ref;
static struct statx_timestamp origin;
static bool ref_set, origin_set;

/*
 * Field IDs, sorted for bsearch() on field_list[].
 */
enum fields {
	stx_atime_tv_nsec,
	stx_atime_tv_sec,
	stx_attributes,
	stx_blksize,
	stx_blocks,
	stx_btime_tv_nsec,
	stx_btime_tv_sec,
	stx_ctime_tv_nsec,
	stx_ctime_tv_sec,
	stx_dev_major,
	stx_dev_minor,
	stx_gid,
	stx_ino,
	stx_mask,
	stx_mode,
	stx_mtime_tv_nsec,
	stx_mtime_tv_sec,
	stx_nlink,
	stx_rdev_major,
	stx_rdev_minor,
	stx_size,
	stx_type,
	stx_uid,
	nr__fields
};

struct field {
	const char *name;		/* Name on command line */
	unsigned int mask_bit;
};

/*
 * List of fields, sorted for bsearch().
 */
static const struct field field_list[nr__fields] = {
	[stx_atime_tv_nsec]	= { "stx_atime.tv_nsec",	STATX_ATIME },
	[stx_atime_tv_sec]	= { "stx_atime.tv_sec",		STATX_ATIME },
	[stx_attributes]	= { "stx_attributes",		0 },
	[stx_blksize]		= { "stx_blksize",		0 },
	[stx_blocks]		= { "stx_blocks",		STATX_BLOCKS },
	[stx_btime_tv_nsec]	= { "stx_btime.tv_nsec",	STATX_BTIME },
	[stx_btime_tv_sec]	= { "stx_btime.tv_sec",		STATX_BTIME },
	[stx_ctime_tv_nsec]	= { "stx_ctime.tv_nsec",	STATX_CTIME },
	[stx_ctime_tv_sec]	= { "stx_ctime.tv_sec",		STATX_CTIME },
	[stx_dev_major]		= { "stx_dev_major",		0 },
	[stx_dev_minor]		= { "stx_dev_minor",		0 },
	[stx_gid]		= { "stx_gid",			STATX_GID },
	[stx_ino]		= { "stx_ino",			STATX_INO },
	[stx_mask]		= { "stx_mask",			0 },
	[stx_mode]		= { "stx_mode",			STATX_MODE },
	[stx_mtime_tv_nsec]	= { "stx_mtime.tv_nsec",	STATX_MTIME },
	[stx_mtime_tv_sec]	= { "stx_mtime.tv_sec",		STATX_MTIME },
	[stx_nlink]		= { "stx_nlink",		STATX_NLINK },
	[stx_rdev_major]	= { "stx_rdev_major",		0 },
	[stx_rdev_minor]	= { "stx_rdev_minor",		0 },
	[stx_size]		= { "stx_size",			STATX_SIZE },
	[stx_type]		= { "stx_type",			STATX_TYPE },
	[stx_uid]		= { "stx_uid",			STATX_UID },
};

static int field_cmp(const void *_key, const void *_p)
{
	const char *key = _key;
	const struct field *p = _p;
	return strcmp(key, p->name);
}

/*
 * Sorted list of attribute flags for bsearch().
 */
struct attr_name {
	const char	*name;
	__u64		attr_flag;
};

static const struct attr_name attr_list[] = {
	{ "append",	STATX_ATTR_APPEND },
	{ "automount",	STATX_ATTR_AUTOMOUNT },
	{ "compressed",	STATX_ATTR_COMPRESSED },
	{ "encrypted",	STATX_ATTR_ENCRYPTED },
	{ "immutable",	STATX_ATTR_IMMUTABLE },
	{ "nodump",	STATX_ATTR_NODUMP },
};

static int attr_name_cmp(const void *_key, const void *_p)
{
	const char *key = _key;
	const struct attr_name *p = _p;
	return strcmp(key, p->name);
}

struct file_type {
	const char *name;
	mode_t mode;
};

/*
 * List of file types.
 */
static const struct file_type file_types[] = {
	{ "fifo",	S_IFIFO },
	{ "char",	S_IFCHR },
	{ "dir",	S_IFDIR },
	{ "block",	S_IFBLK },
	{ "file",	S_IFREG },
	{ "sym",	S_IFLNK },
	{ "sock",	S_IFSOCK },
	{ NULL }
};

static __attribute__((noreturn))
void format(void)
{
	fprintf(stderr, "usage: %s --check-statx\n", prog);
	fprintf(stderr, "usage: %s [-v] [-m<mask>] <testfile> [checks]\n", prog);
	fprintf(stderr, "\t<mask> can be basic, all or a number; all is the default\n");
	fprintf(stderr, "checks is a list of zero or more of:\n");
	fprintf(stderr, "\tattr=[+-]<name> -- check an attribute in stx_attributes\n");
	fprintf(stderr, "\t\tappend -- The file is marked as append only\n");
	fprintf(stderr, "\t\tautomount -- The object is an automount point\n");
	fprintf(stderr, "\t\tcompressed -- The file is marked as compressed\n");
	fprintf(stderr, "\t\tencrypted -- The file is marked as encrypted\n");
	fprintf(stderr, "\t\timmutable -- The file is marked as immutable\n");
	fprintf(stderr, "\t\tnodump -- The file is marked as no-dump\n");
	fprintf(stderr, "\tcmp_ref -- check that the reference file has identical stats\n");
	fprintf(stderr, "\tref=<file> -- get reference stats from file\n");
	fprintf(stderr, "\tstx_<field>=<val> -- statx field value check\n");
	fprintf(stderr, "\tts=<a>,<b> -- timestamp a <= b, where a and b can each be one of:\n");
	fprintf(stderr, "\t\t[abcm] -- the timestamps from testfile\n");
	fprintf(stderr, "\t\t[ABCM] -- the timestamps from the reference file\n");
	fprintf(stderr, "\t\t0 -- the origin timestamp\n");
	fprintf(stderr, "\tts_origin=<sec>.<nsec> -- set the origin timestamp\n");
	fprintf(stderr, "\tts_order -- check the timestamp order\n");
	fprintf(stderr, "\t\t(for stx_type, fifo char dir, block, file, sym, sock can be used)\n");
	exit(2);
}

static __attribute__((noreturn, format(printf, 1, 2)))
void bad_arg(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(2);
}

static __attribute__((format(printf, 1, 2)))
void verbose(const char *fmt, ...)
{
	va_list va;

	if (is_verbose) {
		va_start(va, fmt);
		fputs(" - ", stdout);
		vprintf(fmt, va);
		va_end(va);
	}
}

static __attribute__((format(printf, 2, 3)))
void check(bool good, const char *fmt, ...)
{
	va_list va;

	if (!good) {
		va_start(va, fmt);
		fputs("[!] ", stdout);
		vprintf(fmt, va);
		va_end(va);
		failed = true;
	}
}

/*
 * Compare the contents of a statx struct with that of a stat struct and check
 * that they're the same.
 */
static void cmp_statx(const struct statx *stx, const struct stat *st)
{
#define cmp(fmt, x)						      \
	do {							      \
		check(stx->stx_##x == st->st_##x,		      \
		      "stat.%s differs, "fmt" != "fmt"\n",	      \
		      #x,					      \
		      (unsigned long long)stx->stx_##x,		      \
		      (unsigned long long)st->st_##x);		      \
	} while (0)

	cmp("%llu", blksize);
	cmp("%llu", nlink);
	cmp("%llu", uid);
	cmp("%llu", gid);
	cmp("%llo", mode);
	cmp("%llu", ino);
	cmp("%llu", size);
	cmp("%llu", blocks);

#define devcmp(x) \
	do {								\
		check(stx->stx_##x##_major == major(st->st_##x),	\
		      "stat.%s.major differs, %u != %u\n",		\
		      #x,						\
		      stx->stx_##x##_major,				\
		      major(st->st_##x));				\
		check(stx->stx_##x##_minor == minor(st->st_##x),	\
		      "stat.%s.minor differs, %u != %u\n",		\
		      #x,						\
		      stx->stx_##x##_minor,				\
		      minor(st->st_##x));				\
	} while (0)

	devcmp(dev);
	devcmp(rdev);

#define timecmp(x) \
	do {								\
		check(stx->stx_##x##time.tv_sec == st->st_##x##tim.tv_sec, \
		      "stat.%stime.tv_sec differs, %lld != %lld\n",	\
		      #x,						\
		      (long long)stx->stx_##x##time.tv_sec,		\
		      (long long)st->st_##x##tim.tv_sec);		\
		check(stx->stx_##x##time.tv_nsec == st->st_##x##tim.tv_nsec, \
		      "stat.%stime.tv_nsec differs, %lld != %lld\n",	\
		      #x,						\
		      (long long)stx->stx_##x##time.tv_nsec,		\
		      (long long)st->st_##x##tim.tv_nsec);		\
	} while (0)

	timecmp(a);
	timecmp(c);
	timecmp(m);
}

/*
 * Set origin timestamp from a "<sec>.<nsec>" string.
 */
static void set_origin_timestamp(const char *arg)
{
	long long sec;
	int nsec;

	switch (sscanf(arg, "%lld.%d", &sec, &nsec)) {
	case 0:
		bad_arg("ts_origin= missing seconds value");
	case 1:
		bad_arg("ts_origin= missing nanoseconds value");
	default:
		origin.tv_sec = sec;
		origin.tv_nsec = nsec;
		origin_set = true;
		break;
	}
}

/*
 * Get reference stats from a file.
 */
static void get_reference(const char *file)
{
	int ret;

	if (!*file)
		bad_arg("ref= requires a filename\n");

	memset(&ref, 0xfb, sizeof(ref));
	ret = xfstests_statx(AT_FDCWD, file, AT_SYMLINK_NOFOLLOW,
			     STATX_ATIME | STATX_BTIME | STATX_CTIME | STATX_MTIME,
			     &ref);
	switch (ret) {
	case 0:
		ref_set = true;
		break;
	case -1:
		perror(file);
		exit(1);
	default:
		fprintf(stderr, "Unexpected return %d from statx()\n", ret);
		exit(1);
	}
}

/*
 * Check a pair of timestamps.
 */
static void check_earlier(const struct statx_timestamp *A,
			  const struct statx_timestamp *B,
			  const char *A_name,
			  const char *B_name)
{

	check((B->tv_sec - A->tv_sec) >= 0,
	      "%s.sec is before %s.sec (%lld < %lld)\n",
	      B_name, A_name, B->tv_sec, A->tv_sec);

	if (B->tv_sec == A->tv_sec)
		check((B->tv_nsec - A->tv_nsec) >= 0,
		      "%s.nsec is before %s.nsec (%d < %d)\n",
		      B_name, A_name, B->tv_nsec, A->tv_nsec);
}

/*
 * Check that the timestamps are reasonably ordered.
 *
 * We require the following to hold true immediately after creation if the
 * relevant timestamps exist on the filesystem:
 *
 *	btime <= atime
 *	btime <= mtime <= ctime
 */
static void check_timestamp_order(const struct statx *stx)
{
	if ((stx->stx_mask & (STATX_BTIME | STATX_ATIME)) == (STATX_BTIME | STATX_ATIME))
		check_earlier(&stx->stx_btime, &stx->stx_atime, "btime", "atime");
	if ((stx->stx_mask & (STATX_BTIME | STATX_MTIME)) == (STATX_BTIME | STATX_MTIME))
		check_earlier(&stx->stx_btime, &stx->stx_mtime, "btime", "mtime");
	if ((stx->stx_mask & (STATX_BTIME | STATX_CTIME)) == (STATX_BTIME | STATX_CTIME))
		check_earlier(&stx->stx_btime, &stx->stx_ctime, "btime", "ctime");
	if ((stx->stx_mask & (STATX_MTIME | STATX_CTIME)) == (STATX_MTIME | STATX_CTIME))
		check_earlier(&stx->stx_mtime, &stx->stx_ctime, "mtime", "ctime");
}

/*
 * Check that the second timestamp is the same as or after the first timestamp.
 */
static void check_timestamp(const struct statx *stx, char *arg)
{
	const struct statx_timestamp *a, *b;
	const char *an, *bn;
	unsigned int mask;

	if (strlen(arg) != 3 || arg[1] != ',')
		bad_arg("ts= requires <a>,<b>\n");

	switch (arg[0]) {
	case 'a': a = &stx->stx_atime;	an = "atime";	mask = STATX_ATIME; break;
	case 'b': a = &stx->stx_btime;	an = "btime";	mask = STATX_BTIME; break;
	case 'c': a = &stx->stx_ctime;	an = "ctime";	mask = STATX_CTIME; break;
	case 'm': a = &stx->stx_mtime;	an = "mtime";	mask = STATX_MTIME; break;
	case 'A': a = &ref.stx_atime;	an = "ref_a";	mask = STATX_ATIME; break;
	case 'B': a = &ref.stx_btime;	an = "ref_b";	mask = STATX_BTIME; break;
	case 'C': a = &ref.stx_ctime;	an = "ref_c";	mask = STATX_CTIME; break;
	case 'M': a = &ref.stx_mtime;	an = "ref_m";	mask = STATX_MTIME; break;
	case '0': a = &origin;		an = "origin";	mask = 0; break;
	default:
		bad_arg("ts= timestamp '%c' not supported\n", arg[0]);
	}

	if (arg[0] == '0') {
		if (!origin_set)
			bad_arg("ts= timestamp '%c' requires origin= first\n", arg[0]);
	} else if (arg[0] <= 'Z') {
		if (!ref_set)
			bad_arg("ts= timestamp '%c' requires ref= first\n", arg[0]);
		if (!(ref.stx_mask & mask))
			return;
	} else {
		if (!(stx->stx_mask & mask))
			return;
	}

	switch (arg[2]) {
	case 'a': b = &stx->stx_atime;	bn = "atime";	mask = STATX_ATIME; break;
	case 'b': b = &stx->stx_btime;	bn = "btime";	mask = STATX_BTIME; break;
	case 'c': b = &stx->stx_ctime;	bn = "ctime";	mask = STATX_CTIME; break;
	case 'm': b = &stx->stx_mtime;	bn = "mtime";	mask = STATX_MTIME; break;
	case 'A': b = &ref.stx_atime;	bn = "ref_a";	mask = STATX_ATIME; break;
	case 'B': b = &ref.stx_btime;	bn = "ref_b";	mask = STATX_BTIME; break;
	case 'C': b = &ref.stx_ctime;	bn = "ref_c";	mask = STATX_CTIME; break;
	case 'M': b = &ref.stx_mtime;	bn = "ref_m";	mask = STATX_MTIME; break;
	case '0': b = &origin;		bn = "origin";	mask = 0; break;
	default:
		bad_arg("ts= timestamp '%c' not supported\n", arg[2]);
	}

	if (arg[2] == '0') {
		if (!origin_set)
			bad_arg("ts= timestamp '%c' requires origin= first\n", arg[0]);
	} else if (arg[2] <= 'Z') {
		if (!ref_set)
			bad_arg("ts= timestamp '%c' requires ref= first\n", arg[2]);
		if (!(ref.stx_mask & mask))
			return;
	} else {
		if (!(stx->stx_mask & mask))
			return;
	}

	verbose("check %s <= %s\n", an, bn);
	check_earlier(a, b, an, bn);
}

/*
 * Compare to reference file.
 */
static void cmp_ref(const struct statx *stx, unsigned int mask)
{
#undef cmp
#define cmp(fmt, x)							\
	do {								\
		check(stx->x == ref.x,					\
		      "attr '%s' differs from ref file, "fmt" != "fmt"\n", \
		      #x,						\
		      (unsigned long long)stx->x,			\
		      (unsigned long long)ref.x);			\
	} while (0)

	cmp("%llx", stx_mask);
	cmp("%llx", stx_attributes);
	cmp("%llu", stx_blksize);
	cmp("%llu", stx_attributes);
	cmp("%llu", stx_nlink);
	cmp("%llu", stx_uid);
	cmp("%llu", stx_gid);
	cmp("%llo", stx_mode);
	cmp("%llu", stx_ino);
	cmp("%llu", stx_size);
	cmp("%llu", stx_blocks);
	cmp("%lld", stx_atime.tv_sec);
	cmp("%lld", stx_atime.tv_nsec);
	cmp("%lld", stx_btime.tv_sec);
	cmp("%lld", stx_btime.tv_nsec);
	cmp("%lld", stx_ctime.tv_sec);
	cmp("%lld", stx_ctime.tv_nsec);
	cmp("%lld", stx_mtime.tv_sec);
	cmp("%lld", stx_mtime.tv_nsec);
	cmp("%llu", stx_rdev_major);
	cmp("%llu", stx_rdev_minor);
	cmp("%llu", stx_dev_major);
	cmp("%llu", stx_dev_minor);
}

/*
 * Check an field restriction.  Specified on the command line as a key=val pair
 * in the checks section.  For instance:
 *
 *	stx_type=char
 *	stx_mode=0644
 */
static void check_field(const struct statx *stx, char *arg)
{
	const struct file_type *type;
	const struct field *field;
	unsigned long long ucheck, uval = 0;
	long long scheck, sval = 0;
	char *key, *val, *p;

	verbose("check %s\n", arg);

	key = arg;
	val = strchr(key, '=');
	if (!val || !val[1])
		bad_arg("%s check requires value\n", key);
	*(val++) = 0;

	field = bsearch(key, field_list, nr__fields, sizeof(*field), field_cmp);
	if (!field)
		bad_arg("Field '%s' not supported\n", key);

	/* Read the stat information specified by the key. */
	switch ((enum fields)(field - field_list)) {
	case stx_mask:		uval = stx->stx_mask;		break;
	case stx_blksize:	uval = stx->stx_blksize;	break;
	case stx_attributes:	uval = stx->stx_attributes;	break;
	case stx_nlink:		uval = stx->stx_nlink;		break;
	case stx_uid:		uval = stx->stx_uid;		break;
	case stx_gid:		uval = stx->stx_gid;		break;
	case stx_type:		uval = stx->stx_mode & ~07777;	break;
	case stx_mode:		uval = stx->stx_mode & 07777;	break;
	case stx_ino:		uval = stx->stx_ino;		break;
	case stx_size:		uval = stx->stx_size;		break;
	case stx_blocks:	uval = stx->stx_blocks;		break;
	case stx_rdev_major:	uval = stx->stx_rdev_major;	break;
	case stx_rdev_minor:	uval = stx->stx_rdev_minor;	break;
	case stx_dev_major:	uval = stx->stx_dev_major;	break;
	case stx_dev_minor:	uval = stx->stx_dev_minor;	break;

	case stx_atime_tv_sec:	sval = stx->stx_atime.tv_sec;	break;
	case stx_atime_tv_nsec:	sval = stx->stx_atime.tv_nsec;	break;
	case stx_btime_tv_sec:	sval = stx->stx_btime.tv_sec;	break;
	case stx_btime_tv_nsec:	sval = stx->stx_btime.tv_nsec;	break;
	case stx_ctime_tv_sec:	sval = stx->stx_ctime.tv_sec;	break;
	case stx_ctime_tv_nsec:	sval = stx->stx_ctime.tv_nsec;	break;
	case stx_mtime_tv_sec:	sval = stx->stx_mtime.tv_sec;	break;
	case stx_mtime_tv_nsec:	sval = stx->stx_mtime.tv_nsec;	break;
	default:
		break;
	}

	/* Parse the specified value as signed or unsigned as
	 * appropriate and compare to the stat information.
	 */
	switch ((enum fields)(field - field_list)) {
	case stx_mask:
	case stx_attributes:
		ucheck = strtoull(val, &p, 0);
		if (*p)
			bad_arg("Field '%s' requires unsigned integer\n", key);
		check(uval == ucheck,
		      "%s differs, 0x%llx != 0x%llx\n", key, uval, ucheck);
		break;

	case stx_type:
		for (type = file_types; type->name; type++) {
			if (strcmp(type->name, val) == 0) {
				ucheck = type->mode;
				goto octal_check;
			}
		}

		/* fall through */

	case stx_mode:
		ucheck = strtoull(val, &p, 0);
		if (*p)
			bad_arg("Field '%s' requires unsigned integer\n", key);
	octal_check:
		check(uval == ucheck,
		      "%s differs, 0%llo != 0%llo\n", key, uval, ucheck);
		break;

	case stx_blksize:
	case stx_nlink:
	case stx_uid:
	case stx_gid:
	case stx_ino:
	case stx_size:
	case stx_blocks:
	case stx_rdev_major:
	case stx_rdev_minor:
	case stx_dev_major:
	case stx_dev_minor:
		ucheck = strtoull(val, &p, 0);
		if (*p)
			bad_arg("Field '%s' requires unsigned integer\n", key);
		check(uval == ucheck,
		      "%s differs, %llu != %llu\n", key, uval, ucheck);
		break;

	case stx_atime_tv_sec:
	case stx_atime_tv_nsec:
	case stx_btime_tv_sec:
	case stx_btime_tv_nsec:
	case stx_ctime_tv_sec:
	case stx_ctime_tv_nsec:
	case stx_mtime_tv_sec:
	case stx_mtime_tv_nsec:
		scheck = strtoll(val, &p, 0);
		if (*p)
			bad_arg("Field '%s' requires integer\n", key);
		check(sval == scheck,
		      "%s differs, %lld != %lld\n", key, sval, scheck);
		break;

	default:
		break;
	}
}

/*
 * Check attributes in stx_attributes.  When stx_attributes_mask gets in
 * upstream, we will need to consider that also.
 */
static void check_attribute(const struct statx *stx, char *arg)
{
	const struct attr_name *p;
	__u64 attr;
	bool set;

	verbose("check attr %s\n", arg);
	switch (arg[0]) {
	case '+': set = true;	break;
	case '-': set = false;	break;
	default:
		bad_arg("attr flag must be marked + (set) or - (unset)\n");
	}
	arg++;

	p = bsearch(arg, attr_list, sizeof(attr_list) / sizeof(attr_list[0]),
		    sizeof(attr_list[0]), attr_name_cmp);
	if (!p)
		bad_arg("Unrecognised attr name '%s'\n", arg);

	attr = p->attr_flag;
	if (set) {
		check((stx->stx_attributes & attr) == attr,
		      "Attribute %s should be set\n", arg);
	} else {
		check((stx->stx_attributes & attr) == 0,
		      "Attribute %s should be unset\n", arg);
	}
}

/*
 * Do the testing.
 */
int main(int argc, char **argv)
{
	struct statx stx;
	struct stat st;
	unsigned int mask = STATX_ALL;
	unsigned int atflags = AT_STATX_SYNC_AS_STAT;
	char *p;
	int c, ret;

	if (argc == 2 && strcmp(argv[1], "--check-statx") == 0) {
		errno = 0;
		return (xfstests_statx(AT_FDCWD, "/", 0, 0, &stx) == -1 &&
			errno == ENOSYS) ? 1 : 0;
	}

	prog = argv[0];
	while (c = getopt(argc, argv, "+DFm:v"),
	       c != -1
	       ) {
		switch (c) {
		case 'F':
			atflags &= ~AT_STATX_SYNC_TYPE;
			atflags |= AT_STATX_FORCE_SYNC;
			break;
		case 'D':
			atflags &= ~AT_STATX_SYNC_TYPE;
			atflags |= AT_STATX_DONT_SYNC;
			break;
		case 'm':
			if (strcmp(optarg, "basic") == 0) {
				mask = STATX_BASIC_STATS;
			} else if (strcmp(optarg, "all") == 0) {
				mask = STATX_ALL;
			} else {
				mask = strtoul(optarg, &p, 0);
				if (*p)
					format();
			}
			break;
		case 'v':
			is_verbose = 1;
			break;
		default:
			format();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1)
		format();
	testfile = argv[0];
	argv += 1;

	/* Gather the stats.  We want both statx and stat so that we can
	 * compare what's in the buffers.
	 */
	verbose("call statx %s\n", testfile);
	memset(&stx, 0xfb, sizeof(stx));
	ret = xfstests_statx(AT_FDCWD, testfile, atflags | AT_SYMLINK_NOFOLLOW,
			     mask, &stx);
	switch (ret) {
	case 0:
		break;
	case -1:
		perror(testfile);
		exit(1);
	default:
		fprintf(stderr, "Unexpected return %d from statx()\n", ret);
		exit(1);
	}

	verbose("call stat %s\n", testfile);
	ret = fstatat(AT_FDCWD, testfile, &st, AT_SYMLINK_NOFOLLOW);
	switch (ret) {
	case 0:
		break;
	case -1:
		perror(testfile);
		exit(1);
	default:
		fprintf(stderr, "Unexpected return %d from stat()\n", ret);
		exit(1);
	}

	verbose("compare statx and stat\n");
	cmp_statx(&stx, &st);

	/* Display the available timestamps */
	verbose("begin time %llu.%09u\n", origin.tv_sec, origin.tv_nsec);
	if (stx.stx_mask & STATX_BTIME)
		verbose("     btime %llu.%09u\n", stx.stx_btime.tv_sec, stx.stx_btime.tv_nsec);
	if (stx.stx_mask & STATX_ATIME)
		verbose("     atime %llu.%09u\n", stx.stx_atime.tv_sec, stx.stx_atime.tv_nsec);
	if (stx.stx_mask & STATX_MTIME)
		verbose("     mtime %llu.%09u\n", stx.stx_mtime.tv_sec, stx.stx_mtime.tv_nsec);
	if (stx.stx_mask & STATX_CTIME)
		verbose("     ctime %llu.%09u\n", stx.stx_ctime.tv_sec, stx.stx_ctime.tv_nsec);

	/* Handle additional checks the user specified */
	for (; *argv; argv++) {
		char *arg = *argv;

		if (strncmp("attr=", arg, 5) == 0) {
			/* attr=[+-]<attr> - check attribute flag */
			check_attribute(&stx, arg + 5);
			continue;
		}

		if (strcmp("cmp_ref", arg) == 0) {
			/* cmp_ref - check ref file has same stats */
			cmp_ref(&stx, mask);
			continue;
		}

		if (strncmp(arg, "stx_", 4) == 0) {
			/* stx_<field>=<n> - check field set to n */
			check_field(&stx, *argv);
			continue;
		}

		if (strncmp("ref=", arg, 4) == 0) {
			/* ref=<file> - set reference stats from file */
			get_reference(arg + 4);
			continue;
		}

		if (strcmp("ts_order", arg) == 0) {
			/* ts_order - check timestamp order */
			check_timestamp_order(&stx);
			continue;
		}

		if (strncmp("ts_origin=", arg, 10) == 0) {
			/* ts_origin=<sec>.<nsec> - set origin timestamp */
			set_origin_timestamp(arg + 10);
			continue;
		}

		if (strncmp("ts=", arg, 3) == 0) {
			/* ts=<a>,<b> - check timestamp b is same as a or after */
			check_timestamp(&stx, arg + 3);
			continue;
		}

		bad_arg("check '%s' not supported\n", arg);
	}

	if (failed) {
		printf("Failed\n");
		exit(1);
	}

	verbose("Success\n");
	exit(0);
}
