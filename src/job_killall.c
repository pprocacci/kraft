#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "job.h"

/* kills every background job at once and removes their files; for exit */
void
job_killall(void)
{
  struct job *j;
  int i, ws;

  for (i = 0; i < JOB_MAX; ++i) {
    j = &job_tab[i];
    if (!j->pid) continue;
    kill(-j->pid, SIGKILL);
    if (!j->done)
      while (waitpid(j->pid, &ws, 0) == -1 && errno == EINTR) ;
    unlink(j->path.s);
    j->pid = 0;
  }
}
