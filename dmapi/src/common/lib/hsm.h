/*
 * Defines and structures for our pseudo HSM example
 *
 * This code was written by Peter Lawthers, and placed in the public
 * domain for use by DMAPI implementors and app writers.
 *
 * Standard disclaimer:
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_LIB_HSM_H
#define	_LIB_HSM_H

#ifdef	__cplusplus
extern	"C" {
#endif

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <lib/dmport.h>

#define HANDLE_LEN      64                      /* Swag for this example */
#define HANDLE_STR      ((HANDLE_LEN * 2) + 1)  /* handle as ascii, plus null */
#define IBUFSIZE         1024			/* input buffer size */
#define CHUNKSIZE    	(1024*1024*4) 		/* write size */
#define FENCEPOST_SIZE  ((dm_size_t)(1024*8))
#define ALL_AVAIL_MSGS	0

/*
 * Actions to be performed by the worker bees
 */
#define RESTORE_FILE	"-r"
#define INVAL_FILE	"-i"

#define WORKER_BEE	"wbee"


/*
 * Names of DM attribute used for storing location of a file's data.
 * DM attributes are 8 bytes, including NULL.
 */

#define DLOC_HAN	"dhanloc"		/* staging file handle */
#define DLOC_HANLEN	"dhanlen"		/* staging handle length */

/*
 * Default log file
 */
#define LOG_DEFAULT	"/tmp/dmig_log"

struct ev_name_to_value {
        char            *name;		/* name of event */
        dm_eventtype_t  value;		/* value of event */
};

extern	struct 	ev_name_to_value	ev_names[];
extern	int	ev_namecnt;


struct rt_name_to_value {
        char            *name;		/* name of right */
        dm_right_t	value;		/* value of right */
};

extern	struct 	rt_name_to_value	rt_names[];
extern	int	rt_namecnt;


extern void hantoa(void *hanp, size_t hlen, char *handle_str);
extern int atohan(char *handle_str, void **hanpp, size_t *hlenp);
extern void print_handle(void *hanp, size_t hlen);
extern void print_victim(void *hanp, size_t hlen, dm_off_t fsize);
extern void errno_msg(char *fmt, ...);
extern void err_msg(char *fmt, ...);
extern int setup_dmapi(dm_sessid_t *sid);
extern int get_dmchange(dm_sessid_t sid, void *hanp, size_t hlen,
        dm_token_t token, u_int *change_start);
extern int save_filedata(dm_sessid_t sid, void *hanp, size_t hlen,
        int stg_fd, dm_size_t fsize);
extern int restore_filedata(dm_sessid_t sid, void *hanp, size_t hlen,
        dm_token_t token, void *stg_hanp, size_t stg_hlen, dm_off_t off);

extern void find_test_session(dm_sessid_t *session);

void
print_one_mount_event(
	void		*msg);

int
print_one_message(
	dm_eventmsg_t	*msg);

int
handle_message(
	dm_sessid_t	sid,
	dm_eventmsg_t	*msg);

extern char *date_to_string(
	time_t		timeval);

extern char *mode_to_string(
	mode_t		mode);

extern mode_t field_to_mode(
	mode_t		mode);

extern int validate_state(
	dm_stat_t	*dmstat,
	char		*pathname,
	int		report_errors);

extern char *emask_to_string(
	dm_eventset_t	emask);

extern char *xflags_to_string(
	u_int		xflags);

extern void print_state(
	dm_stat_t	*dmstat);

extern void print_line(
	dm_stat_t	*dmstat);

extern	dm_eventtype_t
ev_name_to_value(
	char		*name);

extern	char *
ev_value_to_name(
	dm_eventtype_t	event);

extern	int
rt_name_to_value(
	char		*name,
	dm_right_t	*rightp);

extern	char *
rt_value_to_name(
	dm_right_t	right);

extern	int
opaque_to_handle(
	char		*name,
	void		**hanpp,
	size_t		*hlenp);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIB_HSM_H */
