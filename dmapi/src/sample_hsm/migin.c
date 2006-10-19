/*
 * Master migration daemon
 *
 * The master migration daemon waits for events on a file and
 * spawns a child process to handle the event
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#include <lib/hsm.h>

extern char	*optarg;
extern int	 optind, optopt, opterr;
extern int	 errno;
char		*Progname;
int		 Verbose;
dm_sessid_t	 sid = DM_NO_SESSION;

extern int	 setup_dmapi(dm_sessid_t *);
extern void	 err_msg(char *, ...);
extern void	 errno_msg(char *, ...);

void		 event_loop(dm_sessid_t);
int		 set_events(dm_sessid_t, void *, size_t);
int		 mk_daemon(char *);
void		 spawn_kid(dm_sessid_t, dm_token_t, char *);
void		 migin_exit(int);
void    	 usage(char *);
void		 setup_signals();


void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s ", prog);
	fprintf(stderr, " <-v verbose> ");
	fprintf(stderr, " <-l logfile> ");
	fprintf(stderr, "filesystem \n");
}


int
main(
	int	argc, 
	char	*argv[])
{
	
	int	 	 c;
	int	 	 error;
	char		*fsname, *logfile;
	void		*fs_hanp;
	size_t		 fs_hlen;


	Progname  = argv[0];
	fsname  = NULL;
	logfile = NULL;

	while ((c = getopt(argc, argv, "vl:")) != EOF) {
		switch (c) {
		case 'v':
			Verbose = 1;
			break;
		case 'l':
			logfile = optarg;
			break;
		case '?':
		default:
			usage(Progname);
			exit(1);
		}
	}
	if (optind >= argc) {
		usage(Progname);
		exit(1);
	}
	fsname = argv[optind];
	if (fsname == NULL) {
		usage(Progname);
		exit(1);
	}
	/*
	 * If no logfile name is specified, we'll just send
	 * all output to some file in /tmp
	 */
	if (logfile == NULL)
		logfile = LOG_DEFAULT;
	 

	/*
	 * Turn ourselves into a daemon
	 */
	if (!Verbose){
		error = mk_daemon(logfile);
		if (error) 
			exit(1);
	}
	setup_signals();
	
	/*
	 * Now we have our filesystem name and possibly a size threshold
	 * to look for. Init the dmapi, and get a filesystem handle so
	 * we can set up our events
	 */
	error = setup_dmapi(&sid);
	if (error) 
		exit(1);
	
	if (dm_path_to_fshandle(fsname, &fs_hanp, &fs_hlen) == -1) {
		errno_msg("Can't get filesystem handle");
		exit(1);
	}


	/*
	 * Set the event disposition so that our session will receive 
	 * the managed region events (read, write, and truncate)
	 */
	error = set_events(sid, fs_hanp, fs_hlen);
	if (error) 
		exit(1);
	

	/*
	 * Now wait forever for messages, spawning kids to
	 * do the actual work
	 */
	event_loop(sid);
	return(0);
}

/*
 * Main event loop processing
 */
void
event_loop(
	dm_sessid_t	sid)
{
	void		*msgbuf;
	size_t	 	 bufsize, rlen;
	int	 	 error;
	dm_eventmsg_t	*msg;

	/*
	 * We take a swag at a buffer size. If it's wrong, we can
	 * always resize it
	 */
	bufsize = sizeof(dm_eventmsg_t) + sizeof(dm_data_event_t) + HANDLE_LEN;
	bufsize *= 16;
	msgbuf  = (void *)malloc(bufsize);
	if (msgbuf == NULL) {
		err_msg("Can't allocate memory for buffer\n");
		goto out;
	}

	for (;;) {	
		error = dm_get_events(sid, ALL_AVAIL_MSGS, DM_EV_WAIT, bufsize,
					msgbuf, &rlen);
		if (error == -1) {
			if (errno == E2BIG) {
				free(msgbuf);
				msgbuf = (void *)malloc(rlen);
				if (msgbuf == NULL) {
					err_msg("Can't resize msg buffer\n");
					goto out;
				}
				continue;
			}
			errno_msg("Error getting events from DMAPI");
			goto out;
		}

		/*
		 * Walk thru the message buffer, pull out each individual
		 * message, and dispatch the messages to child processes
		 * with the sid, token, and data. The children will
		 * respond to the events.
		 */
		msg = (dm_eventmsg_t *)msgbuf;
		while (msg != NULL ) {
			if (Verbose) {
				fprintf(stderr, "Received %s, token %d\n",
				    (msg->ev_type == DM_EVENT_READ ? "read" : 
				    (msg->ev_type == DM_EVENT_WRITE ? "write" : "trunc")), msg->ev_token);
			}
			switch (msg->ev_type) {

			case DM_EVENT_READ:
				spawn_kid(sid, msg->ev_token, RESTORE_FILE);
				break;

			case DM_EVENT_WRITE:
			case DM_EVENT_TRUNCATE:
				spawn_kid(sid, msg->ev_token, INVAL_FILE);
				break;

			default:
				err_msg("Invalid msg type %d\n", msg->ev_type);
				break;
			}
			msg = DM_STEP_TO_NEXT(msg, dm_eventmsg_t *);
		}
	}
out:
	if (msgbuf != NULL)
		free(msgbuf);

	migin_exit(0);
}

/*
 * Fork and exec our worker bee to work on  the file. If 
 * there is any error in fork/exec'ing the file, we have to
 * supply the error return to the event. Once the child gets
 * started, they will respond to the event for us.
 */
void
spawn_kid(
	dm_sessid_t	 sid,
	dm_token_t	 token,
	char		*action)
{
	pid_t	pid;
	char	sidbuf[10];
	char	tokenbuf[10];

	pid = fork();
	if (pid == 0) {
		/*
		 * We're in the child. Try and exec the worker bee
		 */
		sprintf(sidbuf, "%d", sid);
		sprintf(tokenbuf, "%d", token);
		if (Verbose) {
			fprintf(stderr, "execl(%s, %s, %s, -s, %s, -t, %s, 0)\n",
				WORKER_BEE, WORKER_BEE, action, sidbuf,
				tokenbuf);
		}
		execlp("./"WORKER_BEE, WORKER_BEE, action, "-s", sidbuf, 
			"-t", tokenbuf, NULL);
		errno_msg("execlp of worker bee failed");
		(void)dm_respond_event(sid, token, DM_RESP_ABORT, 
					errno, 0, 0);
		exit(1);
	}

	if (pid < 0) {
		err_msg("Can't fork worker bee\n");
		(void)dm_respond_event(sid, token, DM_RESP_ABORT, errno,
					0, 0);
		return;
	}
	return;

}


/*
 * Set the event disposition of the managed region events
 */
int
set_events(
	dm_sessid_t	 sid,
	void		*fs_hanp,
	size_t		 fs_hlen)
{
	dm_eventset_t	eventlist;

	DMEV_ZERO(eventlist);
	DMEV_SET(DM_EVENT_READ, eventlist);
	DMEV_SET(DM_EVENT_WRITE, eventlist);
	DMEV_SET(DM_EVENT_TRUNCATE, eventlist);

	if (dm_set_disp(sid, fs_hanp, fs_hlen, DM_NO_TOKEN, &eventlist, 
			DM_EVENT_MAX) == -1) 
	{
		errno_msg("Can't set event disposition");
		return(1);
	}
	return(0);
}




/*
 * Dissassociate ourselves from our tty, and close all files
 */
int
mk_daemon(
	char	*logfile)
{
	int 			fd;
	int			i;
	struct rlimit		lim;
	pid_t			pid;

	if ((pid = fork()) == -1)
		return (-1);
	if (pid)
		exit(0);

	(void) setsid();

	(void) chdir("/");

	/*
	 * Determine how many open files we've got and close
	 * then all
	 */
	if (getrlimit(RLIMIT_NOFILE, &lim) < 0 ) {
		errno_msg("Can't determine max number of open files");
		return(1);
	}
	for (i=0; i<lim.rlim_cur; i++)
		(void)close(i);

	/*
	 * For error reporting, we re-direct stdout and stderr to a 
	 * logfile. 
	 */
	if ((fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0755)) < 0) {
		errno_msg("Can't open logfile %s", logfile);
		return(1);
	}
	(void)dup2(fd, STDOUT_FILENO);
	(void)dup2(fd, STDERR_FILENO);
	close(fd);

	return(0);
}


void
setup_signals()
{
	struct sigaction	act;

	/*
	 * Set up signals so that we can wait for spawned children 
	 */
	act.sa_handler = migin_exit;
	act.sa_flags   = 0;
	sigemptyset(&act.sa_mask);

	(void)sigaction(SIGHUP, &act, NULL);
	(void)sigaction(SIGINT, &act, NULL);
	(void)sigaction(SIGQUIT, &act, NULL);
	(void)sigaction(SIGTERM, &act, NULL);
	(void)sigaction(SIGUSR1, &act, NULL);
	(void)sigaction(SIGUSR1, &act, NULL);
	(void)sigaction(SIGUSR2, &act, NULL);
}

void
migin_exit(int x)
{
	int		 error;

	/*
	 * We could try and kill off all our kids, but we're not
	 * 'Mr. Mean', so we just wait for them to die.
	 */
	err_msg("%s: waiting for all children to die...\n", Progname);
	while (wait3((int *)0, WNOHANG, (struct rusage *)0) > 0)
		;

	fprintf(stdout, "\n");


	/*
 	 * XXXX 	FIXME		XXX
	 * 
	 *	Should do a set_disp to 0 and then drain the session
	 *	queue as well. On the SGI, we'll need to make the
	 * 	filesystem handle global so that we can get at it
	 */

	error = dm_destroy_session(sid);
	if (error == -1) {
		errno_msg("Can't shut down session\n");
	}

	exit(0);
}

