#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "job.h"
#include "proc.h"
#include "stralloc.h"

/* reaps a finished job, if it has finished */
static void
reap(struct job *j)
{
  int ws;

  if (j->done || waitpid(j->pid, &ws, WNOHANG) != j->pid) return;
  j->done = 1;
  j->status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
}

/*
 * starts cmd in the background.  returns its job number, from 1, or
 * 0 with the reason in err.  a slot whose job has finished is reused
 * when none is free.
 */
int
job_start(const char *cmd, stralloc *err)
{
  struct job *j = 0;
  const char *tmp;
  pid_t pid;
  int i, fd, nul;

  for (i = 0; i < JOB_MAX && !j; ++i)
    if (!job_tab[i].pid) j = &job_tab[i];
  for (i = 0; i < JOB_MAX && !j; ++i) {
    reap(&job_tab[i]);
    if (job_tab[i].done) {
      job_kill(i + 1);
      j = &job_tab[i];
    }
  }
  if (!j) {
    stralloc_copys(err, "too many background jobs; stop one with bash_kill");
    return 0;
  }

  tmp = getenv("TMPDIR");
  if (!stralloc_copys(&j->path, tmp && *tmp ? tmp : "/tmp") ||
      !stralloc_cats(&j->path, "/kraft-job.XXXXXX") || !stralloc_0(&j->path) ||
      !stralloc_copys(&j->cmd, cmd) || !stralloc_0(&j->cmd))
    goto nomem;
  if ((fd = mkstemp(j->path.s)) == -1) {
    stralloc_copys(err, "unable to make an output file: ");
    stralloc_cats(err, strerror(errno));
    return 0;
  }
  pid = fork();
  if (pid == -1) {
    stralloc_copys(err, "unable to fork: ");
    stralloc_cats(err, strerror(errno));
    close(fd);
    unlink(j->path.s);
    return 0;
  }
  if (pid == 0) {
    setsid();
    signal(SIGPIPE, SIG_DFL);
    nul = open("/dev/null", O_RDONLY);
    if (nul != -1) dup2(nul, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    proc_limit();
    execl("/bin/sh", "sh", "-c", cmd, (char *)0);
    _exit(127);
  }
  close(fd);
  j->pid = pid;
  j->done = 0;
  j->status = 0;
  j->off = 0;
  return (int)(j - job_tab) + 1;
nomem:
  stralloc_copys(err, "out of memory");
  return 0;
}
