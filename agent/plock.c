#include "agent.h"

/*
 * in a call running alongside others (see parallel()), takes (on) or
 * gives up the lock they share, for what only one may do at a time:
 * ask the user, number a file for /undo, talk to an MCP server on its
 * pipes.  elsewhere it does nothing.
 */
void
plock(int on)
{
  if (!inchild || lockfd == -1) return;
  lseek(lockfd, 0, SEEK_SET);
  while (lockf(lockfd, on ? F_LOCK : F_ULOCK, 0) == -1 && errno == EINTR) ;
}
