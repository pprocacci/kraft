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

/* all of standard input, as one task */
static void
readall(stralloc *s)
{
  ssize_t r;

  for (;;) {
    if (!stralloc_readyplus(s, 8192)) nomem();
    r = buffer_unixread(0, s->s + s->len, 8192);
    if (r < 0) strerr_sys(111, FATAL, "unable to read standard input", 0);
    if (r == 0) break;
    s->len += (size_t)r;
  }
  while (s->len && (s->s[s->len - 1] == '\n' || s->s[s->len - 1] == ' '))
    --s->len;
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
  atexit(mcpstop);

  argv = options(argc, argv, &resumeit);
  if (script) {
    /* the outcome alone on stdout; the rest on stderr, plainly */
    buffer_flush(buffer_1);
    buffer_1->fd = 2;
    color = 0;
  }
  setconnection();
  mksystem();
  mcpstart();
  settools();
  if (!ctxwin) findwindow();
  if (!stralloc_copys(&cwdsa, getcwd(cwd, sizeof cwd) ? cwd : ".") ||
      !stralloc_0(&cwdsa))
    nomem();
  if (resumeit && !resume()) outs("(no earlier session in this directory)\n");
  if (!sessfn.len) newsession();

  /* a task on the command line, or with -p or -j on stdin: do it, exit */
  if (*argv || script) {
    if (!stralloc_copys(&line, "")) nomem();
    for (; *argv; ++argv) {
      if (line.len && !stralloc_append(&line, ' ')) nomem();
      if (!stralloc_cats(&line, *argv)) nomem();
    }
    if (!line.len) readall(&line);
    if (!line.len) strerr_die(100, FATAL, "no task, on the command line or "
        "standard input", 0);
    task(line.s, line.len);
    nl();
    flush();
    if (script) scriptout(script == 2);
    return status;
  }
  repl();
  return status;
}
