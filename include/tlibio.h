/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define LIO_IO_SYNC             00001   /* read/write */
#define LIO_IO_ASYNC            00002   /* reada/writea/aio_write/aio_read */
#define LIO_IO_SLISTIO          00004   /* single stride sync listio */
#define LIO_IO_ALISTIO          00010   /* single stride async listio */
#define LIO_IO_SYNCV            00020   /* single-buffer readv/writev */
#define LIO_IO_SYNCP            00040   /* pread/pwrite */

#ifdef linux
#define LIO_IO_TYPES            00021   /* all io types */
#endif /* linux */

#define LIO_WAIT_NONE           00010000 /* return asap -- use with care */
#define LIO_WAIT_ACTIVE         00020000 /* spin looking at iosw fields, or EINPROGRESS */
#define LIO_WAIT_RECALL         00040000 /* call recall(2)/aio_suspend(3) */
#define LIO_WAIT_SIGPAUSE       00100000 /* call pause */
#define LIO_WAIT_SIGACTIVE      00200000 /* spin waiting for signal */

#ifdef linux
#define LIO_WAIT_TYPES          00300000 /* all wait types, except nowait */
#endif /* linux */

/* meta wait io  */
/*  00  000 0000 */

#ifdef linux
/* all signal wait types */
#define LIO_WAIT_SIGTYPES	(LIO_WAIT_SIGPAUSE)
#endif /* linux */

/*
 * This bit provides a way to randomly pick an io type and wait method.
 * lio_read_buffer() and lio_write_buffer() functions will call
 * lio_random_methods() with the given method.
 */
#define LIO_RANDOM              010000000

/*
 * This bit provides a way for the programmer to use async i/o with
 * signals and to use their own signal handler.  By default,
 * the signal will only be given to the system call if the wait
 * method is LIO_WAIT_SIGPAUSE or LIO_WAIT_SIGACTIVE.
 * Whenever these wait methods are used, libio signal handler
 * will be used.
 */
#define LIO_USE_SIGNAL          020000000

/*
 * prototypes/structures for functions in the libio.c module.  See comments
 * in that module, or man page entries for information on the individual
 * functions.
 */

int  stride_bounds(int offset, int stride, int nstrides,
		      int bytes_per_stride, int *min_byte, int *max_byte);

int  lio_set_debug(int level);
int  lio_parse_io_arg1(char *string);
void lio_help1(char *prefex);
int  lio_parse_io_arg2(char *string, char **badtoken);
void lio_help2(char *prefex);
int  lio_write_buffer(int fd, int method, char *buffer, int size,
		      int sig, char **errmsg, long wrd);

int  lio_read_buffer(int fd, int method, char *buffer, int size,
		     int sig, char **errmsg, long wrd);
int  lio_random_methods(long mask);

/*
 * Define the structure that contains the infomation that is used
 * by the parsing and help functions.
 */
struct lio_info_type {
    char *token;
    int  bits;
    char *desc;
};


