#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "mcp.h"

/* closes m's stdin, which asks it to exit, and makes sure that it does */
void
mcp_stop(struct mcp *m)
{
  int i;

  if (!m->pid) return;
  close(m->to);
  close(m->from);
  for (i = 0; i < 20 && waitpid(m->pid, 0, WNOHANG) == 0; ++i)
    usleep(10000);
  if (i == 20) {
    kill(-m->pid, SIGTERM);
    kill(m->pid, SIGTERM);
    waitpid(m->pid, 0, 0);
  }
  m->pid = 0;
}
