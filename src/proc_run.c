#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "proc.h"
#include "stralloc.h"

/*
 * limits for what the model runs, generous enough for big builds:
 * CPU seconds per process, the largest file it may write, the user's
 * processes (bounding a fork bomb), and no core dumps.  a limit is
 * only ever lowered.
 */
static const struct { int res; rlim_t max; } limits[] = {
  { RLIMIT_CPU, 3600 },
  { RLIMIT_FSIZE, (rlim_t)4 << 30 },
  { RLIMIT_NPROC, 2048 },
  { RLIMIT_CORE, 0 },
};

static void
limit(void)
{
  struct rlimit rl;
  size_t i;

  for (i = 0; i < sizeof limits / sizeof limits[0]; ++i) {
    if (getrlimit(limits[i].res, &rl) == -1) continue;
    if (rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur > limits[i].max)
      rl.rlim_cur = limits[i].max;
    if (rl.rlim_max == RLIM_INFINITY || rl.rlim_max > limits[i].max)
      rl.rlim_max = limits[i].max;
    setrlimit(limits[i].res, &rl);
  }
}

static long
msleft(const struct timespec *end)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (long)(end->tv_sec - now.tv_sec) * 1000 +
      (end->tv_nsec - now.tv_nsec) / 1000000;
}

/*
 * runs argv[0] (a full path) with stdin from /dev/null and stdout
 * and stderr captured into out, keeping at most cap bytes.  the
 * program gets its own process group, which is killed if it runs
 * longer than secs seconds, and the resource limits above.  *status is the exit code, 128 plus the
 * signal number if it was killed by a signal, or -1 if it timed out.
 * returns 1 if it ran, 0 with errno set if it could not be started.
 */
int
proc_run(char *const *argv, unsigned long secs, size_t cap, stralloc *out,
    int *status)
{
  struct timespec end;
  struct pollfd pf;
  char buf[4096];
  size_t dropped = 0, k;
  ssize_t r;
  long ms;
  pid_t pid;
  int p[2], fd, ws, timedout = 0;

  if (pipe2(p, O_CLOEXEC) == -1) return 0;
  pid = fork();
  if (pid == -1) {
    close(p[0]);
    close(p[1]);
    return 0;
  }
  if (pid == 0) {
    setsid();
    signal(SIGPIPE, SIG_DFL);
    fd = open("/dev/null", O_RDONLY);
    if (fd != -1) dup2(fd, 0);
    dup2(p[1], 1);
    dup2(p[1], 2);
    limit();
    execv(argv[0], argv);
    _exit(127);
  }
  close(p[1]);
  proc_pid = pid;

  clock_gettime(CLOCK_MONOTONIC, &end);
  end.tv_sec += (time_t)secs;
  pf.fd = p[0];
  pf.events = POLLIN;
  for (;;) {
    ms = msleft(&end);
    if (ms <= 0) {
      timedout = 1;
      kill(-pid, SIGKILL);
      break;
    }
    if (poll(&pf, 1, ms > INT_MAX ? INT_MAX : (int)ms) == -1) {
      if (errno == EINTR) continue;
      break;
    }
    if (!pf.revents) continue;
    r = read(p[0], buf, sizeof buf);
    if (r == -1 && errno == EINTR) continue;
    if (r <= 0) break;
    k = (size_t)r;
    if (out->len < cap) {
      if (k > cap - out->len) k = cap - out->len;
      if (!stralloc_catb(out, buf, k)) break;
    } else
      k = 0;
    dropped += (size_t)r - k;
  }
  close(p[0]);
  while (waitpid(pid, &ws, 0) == -1 && errno == EINTR) ;
  proc_pid = 0;

  if (dropped) {
    stralloc_cats(out, "\n[");
    stralloc_catulong(out, (unsigned long)dropped);
    stralloc_cats(out, " more bytes of output not shown]");
  }
  if (timedout) *status = -1;
  else if (WIFEXITED(ws)) *status = WEXITSTATUS(ws);
  else *status = 128 + WTERMSIG(ws);
  return 1;
}
