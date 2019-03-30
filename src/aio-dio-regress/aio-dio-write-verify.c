// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Red Hat, Inc. All Rights reserved.
 *
 * This program is used to do AIO write test. Each <-a size=N,off=M> field
 * specifies an AIO write. More this kind of fields will be combined to a
 * group of AIO write, then io_submit them together. All AIO write field can
 * overlap or be sparse.
 *
 * After all AIO write operations done, each [size, off] content will be read
 * back, verify if there's corruption.
 *
 * Before doing a series of AIO write, an optional ftruncate operation can be
 * chosen. To truncate the file i_size to a specified location for testing.
 *
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include <libaio.h>

#define MAX_EVENT_NUM	128
/*
 * Use same fill pattern each time, so even overlap write won't get
 * different content
 */
#define IO_PATTERN	0x5a

void usage(char *progname)
{
	fprintf(stderr, "usage: %s [-t truncsize ] <-a size=N,off=M [-a ...]>  filename\n"
	        "\t-t truncsize: truncate the file to a special size before AIO wirte\n"
	        "\t-a: specify once AIO write size and startoff, this option can be specified many times, but less than 128\n"
	        "\t\tsize=N: AIO write size\n"
	        "\t\toff=M:  AIO write startoff\n"
	        "e.g: %s -t 4608 -a size=4096,off=512 -a size=4096,off=4608 filename\n",
	        progname, progname);
	exit(1);
}

void dump_buffer(void *buf, off64_t offset, ssize_t len)
{
	int	i, j;
	char	*p;
	int	new;

	for (i = 0, p = (char *)buf; i < len; i += 16) {
		char    *s = p;

		if (i && !memcmp(p, p - 16, 16)) {
			new = 0;
		} else {
			if (i)
				printf("*\n");
			new = 1;
		}

		if (!new) {
			p += 16;
			continue;
		}

		printf("%08llx  ", (unsigned long long)offset + i);
		for (j = 0; j < 16 && i + j < len; j++, p++)
			printf("%02x ", *p);
		printf(" ");
		for (j = 0; j < 16 && i + j < len; j++, s++) {
			if (isalnum((int)*s))
				printf("%c", *s);
			else
				printf(".");
		}
		printf("\n");

	}
	printf("%08llx\n", (unsigned long long)offset + i);
}

/* Parameters for once AIO write&verify testing */
struct io_params {
	void          *buf;
	void          *cmp_buf;
	unsigned long buf_size;	/* the size of AIO write */
	unsigned long offset;	/* AIO write offset*/
};

struct io_params_node {
	struct iocb iocb;
	struct io_params *param;
	struct io_params_node *next;
};

struct io_params_node *iop_head = NULL;

/* Suboptions of '-a' option */
enum {
	BFS_OPT = 0,
	OFS_OPT
};

char *const token[] = {
	[BFS_OPT]	= "size",	/* buffer size of once AIO write */
	[OFS_OPT]	= "off",	/* start offset */
	NULL
};

/*
 * Combine each AIO write operation things to a linked list.
 *
 * Note: There won't be link structure free, due to the process will
 * exit directly when hit a error)
 */
static int add_io_param(unsigned long bs, unsigned long off)
{
	struct io_params_node *io = NULL;
	struct io_params_node **p = &iop_head;

	io = malloc(sizeof(struct io_params_node));
	if (!io)
		goto err_out;

	io->param = malloc(sizeof(struct io_params));
	if (!io->param)
		goto err_out;

	io->param->buf_size = bs;
	io->param->offset = off;

	io->next = NULL;

	if (bs > 0) {
		if (posix_memalign(&io->param->buf, getpagesize(), bs))
			goto err_out;
		io->param->cmp_buf = malloc(bs);
		if (io->param->cmp_buf == NULL)
			goto err_out;
		memset(io->param->buf, IO_PATTERN, bs);
		memset(io->param->cmp_buf, IO_PATTERN, bs);
	} else {
		return 1;
	}

	/* find the tail */
	while(*p != NULL) {
		p = &((*p)->next);
	}
	*p = io;

	return 0;

 err_out:
	perror("alloc memory");
	return 1;
}

static int parse_subopts(char *arg)
{
	char *p = arg;
	char *value;
	unsigned long bsize = 0;
	unsigned long off = 0;

	while (*p != '\0') {
		char *endp;

		switch(getsubopt(&p, token, &value)) {
		case BFS_OPT:
			bsize = strtoul(value, &endp, 0);
			break;
		case OFS_OPT:
			off = strtoul(value, &endp, 0);
			break;
		default:
			fprintf(stderr, "No match found for subopt %s\n",
			        value);
			return 1;
		}
	}

	return add_io_param(bsize, off);
}

static int io_write(int fd, int num_events)
{
	int err;
	struct io_params_node *p = iop_head;
	struct iocb **iocbs;
	struct io_event *evs;
	struct io_context *ctx = NULL;
	int i;

	err = io_setup(num_events, &ctx);
	if (err) {
		perror("io_setup");
		return 1;
	}

	iocbs = (struct iocb **)malloc(num_events * sizeof(struct iocb *));
	if (iocbs == NULL) {
		perror("malloc");
		return 1;
	}

	evs = malloc(num_events * sizeof(struct io_event));
	if (evs == NULL) {
		perror("malloc");
		return 1;
	}

	i = 0;
	while (p != NULL) {
		iocbs[i] = &(p->iocb);
		io_prep_pwrite(&p->iocb, fd, p->param->buf,
		               p->param->buf_size, p->param->offset);
		p = p->next;
		i++;
	}

	err = io_submit(ctx, num_events, iocbs);
	if (err != num_events) {
		fprintf(stderr, "error %s during %s\n",
		        strerror(err),
		        "io_submit");
		return 1;
	}

	err = io_getevents(ctx, num_events, num_events, evs, NULL);
	if (err != num_events) {
		fprintf(stderr, "error %s during %s\n",
		        strerror(err),
		        "io_getevents");
		return 1;
	}

	/* Try to destroy at here, not necessary, so don't check result */
	io_destroy(ctx);

	return 0;
}

static int io_verify(int fd)
{
	struct io_params_node *p = iop_head;
	ssize_t sret;
	int corrupted = 0;

	while(p != NULL) {
		sret = pread(fd, p->param->buf,
		             p->param->buf_size, p->param->offset);
		if (sret == -1) {
			perror("pread");
			return 1;
		} else if (sret != p->param->buf_size) {
			fprintf(stderr, "short read %zd was less than %zu\n",
			        sret, p->param->buf_size);
			return 1;
		}
		if (memcmp(p->param->buf,
		           p->param->cmp_buf, p->param->buf_size)) {
			printf("Find corruption\n");
			dump_buffer(p->param->buf, p->param->offset,
			            p->param->buf_size);
			corrupted++;
		}
		p = p->next;
	}

	return corrupted;
}

int main(int argc, char *argv[])
{
	int fd;
	int c;
	char *filename = NULL;
	int num_events = 0;
	off_t tsize = 0;

	while ((c = getopt(argc, argv, "a:t:")) != -1) {
		char *endp;

		switch (c) {
		case 'a':
			if (parse_subopts(optarg) == 0) {
				num_events++;
			} else {
				fprintf(stderr, "Bad subopt %s\n", optarg);
				usage(argv[0]);
			}
			break;
		case 't':
			tsize = strtoul(optarg, &endp, 0);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (num_events > MAX_EVENT_NUM) {
		fprintf(stderr, "Too many AIO events, %d > %d\n",
		        num_events, MAX_EVENT_NUM);
		usage(argv[0]);
	}

	if (optind == argc - 1)
		filename = argv[optind];
	else
		usage(argv[0]);

	fd = open(filename, O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	if (tsize > 0) {
		if (ftruncate(fd, tsize)) {
			perror("ftruncate");
			return 1;
		}
	}

	if (io_write(fd, num_events) != 0) {
		fprintf(stderr, "AIO write fails\n");
		return 1;
	}

	if (io_verify(fd) != 0) {
		fprintf(stderr, "Data verification fails\n");
		return 1;
	}

	close(fd);
	return 0;
}
