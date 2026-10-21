#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include "job.h"
#include "stralloc.h"

static void
reap(struct job *j)
{
  int ws;

  if (j->done || waitpid(j->pid, &ws, WNOHANG) != j->pid) return;
  j->done = 1;
  j->status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
}

/*
 * appends job id's output since the last call to out, at most cap
 * bytes, and then its state.  if there is no new output and it is
 * still running, waits up to secs seconds for some (^C cuts the wait
 * short).  returns 1, or 0 if there is no such job.
 */
int
job_output(int id, unsigned long secs, size_t cap, stralloc *out)
{
  struct job *j;
  struct stat st;
  struct timespec end, now;
  char buf[8192];
  size_t want;
  ssize_t r;
  int fd;

  if (id < 1 || id > JOB_MAX || !job_tab[id - 1].pid) return 0;
  j = &job_tab[id - 1];
  if ((fd = open(j->path.s, O_RDONLY | O_CLOEXEC)) == -1) return 0;

  clock_gettime(CLOCK_MONOTONIC, &end);
  end.tv_sec += (time_t)secs;
  for (;;) {
    reap(j);
    if (fstat(fd, &st) == -1 || st.st_size > j->off || j->done) break;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > end.tv_sec ||
        (now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec))
      break;
    if (poll(0, 0, 200) == -1 && errno == EINTR) break;
  }

  if (lseek(fd, j->off, SEEK_SET) != -1)
    while (cap) {
      want = cap < sizeof buf ? cap : sizeof buf;
      r = read(fd, buf, want);
      if (r <= 0) break;
      if (!stralloc_catb(out, buf, (size_t)r)) break;
      j->off += r;
      cap -= (size_t)r;
    }
  if (fstat(fd, &st) == 0 && st.st_size > j->off) {
    stralloc_cats(out, "\n[more output follows; call bash_output again]");
  }
  close(fd);

  if (out->len && out->s[out->len - 1] != '\n') stralloc_append(out, '\n');
  stralloc_cats(out, "[job ");
  stralloc_catulong(out, (unsigned long)id);
  if (j->done) {
    stralloc_cats(out, " exited with status ");
    stralloc_catulong(out, (unsigned long)j->status);
  } else
    stralloc_cats(out, " is still running");
  stralloc_cats(out, "]");
  return 1;
}
