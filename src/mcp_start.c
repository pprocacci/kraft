#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "mcp.h"
#include "stralloc.h"

/*
 * starts m's command with /bin/sh, in a session of its own so that an
 * interrupt at the terminal does not reach it.  its stderr goes to
 * file log (or /dev/null if log is 0).  returns 1, or 0 with the
 * reason in why.
 */
int
mcp_start(struct mcp *m, const char *log, stralloc *why)
{
  int in[2], out[2], fd;
  pid_t pid;

  if (pipe(in) == -1) goto sys;
  if (pipe(out) == -1) {
    close(in[0]);
    close(in[1]);
    goto sys;
  }
  if ((pid = fork()) == -1) {
    close(in[0]);
    close(in[1]);
    close(out[0]);
    close(out[1]);
    goto sys;
  }
  if (pid == 0) {
    setsid();
    signal(SIGPIPE, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    dup2(in[0], 0);
    dup2(out[1], 1);
    fd = open(log ? log : "/dev/null", log ? O_WRONLY | O_CREAT | O_TRUNC :
        O_WRONLY, 0600);
    if (fd != -1) dup2(fd, 2);
    if (fd > 2) close(fd);
    close(in[0]);
    close(in[1]);
    close(out[0]);
    close(out[1]);
    execl("/bin/sh", "sh", "-c", m->cmd, (char *)0);
    _exit(127);
  }
  close(in[0]);
  close(out[1]);
  fcntl(in[1], F_SETFD, FD_CLOEXEC);
  fcntl(out[0], F_SETFD, FD_CLOEXEC);
  m->pid = pid;
  m->to = in[1];
  m->from = out[0];
  m->id = 0;
  m->buf.len = 0;
  return 1;
sys:
  stralloc_copys(why, strerror(errno));
  return 0;
}
