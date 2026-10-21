#include "agent/agent.h"

/*
 * ^C: stop the reply by shutting its connection down, or stop the
 * running command (a second ^C kills it outright).  only
 * async-signal-safe calls here.
 */
static void
onint(int sig)
{
  int e = errno;

  (void)sig;
  if (intr < 2) ++intr;
  if (curfd != -1) shutdown(curfd, SHUT_RDWR);
  if (proc_pid > 0) kill(-proc_pid, intr > 1 ? SIGKILL : SIGINT);
  errno = e;
}

int
main(int argc, char **argv)
{
  struct sigaction sa;
  char cwd[PATH_MAX];
  int resumeit;

  signal(SIGPIPE, SIG_IGN);
  /* colour for a terminal, unless NO_COLOR (no-color.org) says not */
  color = isatty(1) && !getenv("NO_COLOR") && env("TERM", 0) &&
      str_diff(env("TERM", ""), "dumb");
  sa.sa_handler = onint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  /* no SA_RESTART: blocked calls must return EINTR */
  sigaction(SIGINT, &sa, 0);
  atexit(job_killall);  /* background commands do not outlive kraft */

  argv = options(argc, argv, &resumeit);
  setconnection();
  mksystem();
  settools();
  if (!ctxwin) findwindow();
  if (!stralloc_copys(&cwdsa, getcwd(cwd, sizeof cwd) ? cwd : ".") ||
      !stralloc_0(&cwdsa))
    nomem();
  if (resumeit && !resume()) outs("(no earlier session in this directory)\n");
  if (!sessfn.len) newsession();

  /* a task on the command line: do it and exit */
  if (*argv) {
    if (!stralloc_copys(&line, "")) nomem();
    for (; *argv; ++argv) {
      if (line.len && !stralloc_append(&line, ' ')) nomem();
      if (!stralloc_cats(&line, *argv)) nomem();
    }
    task(line.s, line.len);
    nl();
    flush();
    return status;
  }
  repl();
  return status;
}
