/*
 * Test program based on Linux mount(8) source attempting to
 * trigger a suspected problem in rename(2) code paths - its
 * symptoms have been multiple mtab entries in /etc... hence
 * use of the actual mount code here.
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <mntent.h>
#include <limits.h>

#define LOCK_TIMEOUT	10
#define _(x)		(x)

static char *mounted = "t_mtab";
static char *mounted_lock = "t_mtab~";
static char *mounted_temp = "t_mtab.tmp";

/* Updating mtab ----------------------------------------------*/

/* Flag for already existing lock file. */
static int we_created_lockfile = 0;

/* Flag to indicate that signals have been set up. */
static int signals_have_been_setup = 0;

/* Ensure that the lock is released if we are interrupted.  */
static void
handler (int sig) {
    fprintf(stderr, "%s", sys_siglist[sig]);
    exit(1);
}

static void
setlkw_timeout (int sig) {
     /* nothing, fcntl will fail anyway */
}

/* Create the lock file.
   The lock file will be removed if we catch a signal or when we exit. */
/* The old code here used flock on a lock file /etc/mtab~ and deleted
   this lock file afterwards. However, as rgooch remarks, that has a
   race: a second mount may be waiting on the lock and proceed as
   soon as the lock file is deleted by the first mount, and immediately
   afterwards a third mount comes, creates a new /etc/mtab~, applies
   flock to that, and also proceeds, so that the second and third mount
   now both are scribbling in /etc/mtab.
   The new code uses a link() instead of a creat(), where we proceed
   only if it was us that created the lock, and hence we always have
   to delete the lock afterwards. Now the use of flock() is in principle
   superfluous, but avoids an arbitrary sleep(). */

void
lock_mtab (void) {
#if 0	/* nathans: dont limit, we are forcing lots of parallel accesses */
	int tries = 3;
#endif
	char linktargetfile[PATH_MAX + 20];

	if (!signals_have_been_setup) {
		int sig = 0;
		struct sigaction sa;

		sa.sa_handler = handler;
		sa.sa_flags = 0;
		sigfillset (&sa.sa_mask);
  
		while (sigismember (&sa.sa_mask, ++sig) != -1
		       && sig != SIGCHLD) {
			if (sig == SIGALRM)
				sa.sa_handler = setlkw_timeout;
			else
				sa.sa_handler = handler;
			sigaction (sig, &sa, (struct sigaction *) 0);
		}
		signals_have_been_setup = 1;
	}

	/* use 20 as upper bound for the length of %d output */
	snprintf(linktargetfile, PATH_MAX+20, "%s%d", mounted_lock, getpid());

	/* Repeat until it was us who made the link */
	while (!we_created_lockfile) {
		struct flock flock;
		int fd, errsv, i, j;

		i = open (linktargetfile, O_WRONLY|O_CREAT, 0);
		if (i < 0) {
			int errsv = errno;
			/* linktargetfile does not exist (as a file)
			   and we cannot create it. Read-only filesystem?
			   Too many files open in the system?
			   Filesystem full? */
			fprintf(stderr, "can't create lock file %s: %s "
			     "(use -n flag to override)",
			     linktargetfile, strerror (errsv));
			exit(1);
		}
		close(i);

		j = link(linktargetfile, mounted_lock);
		errsv = errno;

		(void) unlink(linktargetfile);

		if (j < 0 && errsv != EEXIST) {
			fprintf(stderr, "can't link lock file %s: %s "
			     "(use -n flag to override)",
			     mounted_lock, strerror (errsv));
			exit(1);
		}

		fd = open (mounted_lock, O_WRONLY);

		if (fd < 0) {
			int errsv = errno;
			/* Strange... Maybe the file was just deleted? */
#if 0	/* nathans: dont limit, we are forcing lots of parallel accesses */
			if (errno == ENOENT && tries-- > 0)
#endif
			if (errno == ENOENT)
				continue;
			fprintf(stderr, "can't open lock file %s: %s\n",
			     mounted_lock, strerror (errsv));
			exit(1);
		}

		flock.l_type = F_WRLCK;
		flock.l_whence = SEEK_SET;
		flock.l_start = 0;
		flock.l_len = 0;

		if (j == 0) {
			/* We made the link. Now claim the lock. */
			if (fcntl (fd, F_SETLK, &flock) == -1 &&
			    errno != EBUSY && errno != EAGAIN) {
				int errsv = errno;
				printf(_("Can't lock lock file %s: %s\n"),
					   mounted_lock, strerror (errsv));
				/* proceed anyway */
			}
			we_created_lockfile = 1;
		} else {
#if 0	/* nathans: dont limit, we are forcing lots of parallel accesses */
			static int tries = 0;
#endif

			/* Someone else made the link. Wait. */
			alarm(LOCK_TIMEOUT);
			if (fcntl (fd, F_SETLKW, &flock) == -1 &&
			    errno != EBUSY && errno != EAGAIN) {
				int errsv = errno;
				fprintf(stderr, "can't lock lock file %s: %s",
				     mounted_lock, (errno == EINTR) ?
				     _("timed out") : strerror (errsv));
				exit(1);
			}
			alarm(0);
#if 0	/* nathans: dont limit, we are forcing lots of parallel accesses */
			/* Limit the number of iterations - maybe there
			   still is some old /etc/mtab~ */
			if (tries++ > 3) {
				if (tries > 5) {
					fprintf(stderr, "Cant create link %s\n"
					    "Perhaps there is a stale lock file?\n",
					     mounted_lock);
					exit(1);
				}
				sleep(1);
			}
#endif
		}
		close (fd);
	}
}

/* Remove lock file.  */
void
unlock_mtab (void) {
	if (we_created_lockfile) {
		unlink (mounted_lock);
		we_created_lockfile = 0;
	}
}

/*
 * Update the mtab.
 *  Used by umount with null INSTEAD: remove the last DIR entry.
 *  Used by mount upon a remount: update option part,
 *   and complain if a wrong device or type was given.
 *   [Note that often a remount will be a rw remount of /
 *    where there was no entry before, and we'll have to believe
 *    the values given in INSTEAD.]
 */

void
update_mtab (void)
{
	FILE *mntent_fp, *mftmp;
	char buffer[4096];
	int size;

	lock_mtab();

	/* having locked mtab, read it again & write to mtemp */
	mntent_fp = fopen(mounted, "r");
	if (!mntent_fp) {
		fprintf(stderr, "cannot open %s for reading\n", mounted);
		exit(1);
	}
	mftmp = fopen(mounted_temp, "w");
	if (!mftmp) {
		fprintf(stderr, "cannot open %s for writing\n", mounted_temp);
		exit(1);
	}
	while ((size = read(fileno(mntent_fp), buffer, sizeof(buffer))) > 0) {
		if (write(fileno(mftmp), buffer, size) < 0) {
			fprintf(stderr, "write failure: %s\n", strerror(errno));
			exit(1);
		}
	}
	if (size < 0) {
		fprintf(stderr, "read failure: %s\n", strerror(errno));
		exit(1);
	}
	fclose(mntent_fp);

	if (fchmod (fileno (mftmp),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0) {
		int errsv = errno;
		fprintf(stderr, _("error changing mode of %s: %s\n"),
			mounted_temp, strerror (errsv));
	}
	fclose(mftmp);

	{ /*
	   * If mount is setuid and some non-root user mounts sth,
	   * then mtab.tmp might get the group of this user. Copy uid/gid
	   * from the present mtab before renaming.
	   */
	    struct stat sbuf;
	    if (stat (mounted, &sbuf) == 0)
		chown (mounted_temp, sbuf.st_uid, sbuf.st_gid);
	}

	/* rename mtemp to mtab */
	if (rename (mounted_temp, mounted) < 0) {
		int errsv = errno;
		fprintf(stderr, _("can't rename %s to %s: %s\n"),
			mounted_temp, mounted, strerror(errsv));
	}

	unlock_mtab();
}

int main(int argc, char **argv)
{
	int i, stop = 100000;
	FILE *fout = NULL;

	if (argc > 1)
		stop = atoi(argv[1]);

	for (i = 0; i < stop; i++) {
		update_mtab();
	}

	if (argc > 2)
		fout = fopen(argv[2],"a");
	if (!fout)
		fout = stdout;
	fprintf(fout, "completed %d iterations\n", stop);
	return 0;
}
