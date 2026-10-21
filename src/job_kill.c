#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include "job.h"
#include "stralloc.h"

/*
 * stops job id (SIGTERM to its process group, SIGKILL two seconds
 * later if need be), removes its output file and frees its slot.
 * returns 1, or 0 if there is no such job.
 */
int
job_kill(int id)
{
  struct job *j;
  int ws, i;

  if (id < 1 || id > JOB_MAX || !job_tab[id - 1].pid) return 0;
  j = &job_tab[id - 1];
  if (!j->done) {
    kill(-j->pid, SIGTERM);
    for (i = 0; i < 20; ++i) {
      if (waitpid(j->pid, &ws, WNOHANG) == j->pid) { j->done = 1; break; }
      poll(0, 0, 100);
    }
    if (!j->done) {
      kill(-j->pid, SIGKILL);
      while (waitpid(j->pid, &ws, 0) == -1 && errno == EINTR) ;
    }
  }
  kill(-j->pid, SIGKILL);  /* anything it left behind in its group */
  unlink(j->path.s);
  j->pid = 0;
  j->done = 0;
  j->off = 0;
  return 1;
}
