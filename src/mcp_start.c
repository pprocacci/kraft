#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "byte.h"
#include "mcp.h"
#include "str.h"
#include "stralloc.h"

/*
 * starts m's command with /bin/sh, in a session of its own so that an
 * interrupt at the terminal does not reach it.  its stderr goes to
 * file log (or /dev/null if log is 0).  for an HTTP server there is
 * nothing to start.  returns 1, or 0 with the reason in why.
 */
int
mcp_start(struct mcp *m, const char *log, stralloc *why)
{
  int in[2], out[2], fd;
  pid_t pid;

  m->id = 0;
  m->buf.len = 0;
  m->session.len = 0;
  m->http = str_start(m->cmd, "http://") || str_start(m->cmd, "https://");
  if (m->http) {
    /* nothing to start: the first request will tell */
    if (!url_parse(&m->u, m->cmd)) {
      stralloc_copys(why, "not a URL kraft understands");
      return 0;
    }
    if (!stralloc_copys(&m->hosthdr, "")) goto nomem;
    if (byte_chr(m->u.host.s, m->u.host.len, ':') < m->u.host.len - 1 &&
        (!stralloc_cats(&m->hosthdr, "[") ||
        !stralloc_cats(&m->hosthdr, m->u.host.s) ||
        !stralloc_cats(&m->hosthdr, "]")))
      goto nomem;
    else if (!m->hosthdr.len && !stralloc_cats(&m->hosthdr, m->u.host.s))
      goto nomem;
    if (str_diff(m->u.port.s, m->u.tls ? "443" : "80") &&
        (!stralloc_cats(&m->hosthdr, ":") ||
        !stralloc_cats(&m->hosthdr, m->u.port.s)))
      goto nomem;
    if (!stralloc_0(&m->hosthdr)) goto nomem;
    m->up = 1;
    return 1;
  }

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
  m->up = 1;
  return 1;
sys:
  stralloc_copys(why, strerror(errno));
  return 0;
nomem:
  stralloc_copys(why, "out of memory");
  return 0;
}
