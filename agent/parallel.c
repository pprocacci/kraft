#include "agent.h"

#define MAXPAR 8

/* a child's last words: how it went, what it cost, then the output */
static void
report(int fd, int ok, const struct usage *u0, unsigned long long c0,
    unsigned long r0, const stralloc *o)
{
  stralloc h = STRALLOC_ZERO;

  if (stralloc_copys(&h, ok ? "1 " : "0 ") &&
      stralloc_catulong(&h, requests - r0) && stralloc_cats(&h, " ") &&
      stralloc_catulong(&h, (unsigned long)(cost - c0)) &&
      stralloc_cats(&h, costgap ? " 1 " : " 0 ") &&
      stralloc_catulong(&h, total.in - u0->in) && stralloc_cats(&h, " ") &&
      stralloc_catulong(&h, total.cread - u0->cread) && stralloc_cats(&h, " ") &&
      stralloc_catulong(&h, total.cwrite - u0->cwrite) &&
      stralloc_cats(&h, " ") &&
      stralloc_catulong(&h, total.out - u0->out) && stralloc_cats(&h, "\n"))
    if (buffer_unixwrite(fd, h.s, h.len) == (ssize_t)h.len)
      buffer_unixwrite(fd, o->s, o->len);
}

/* the numbers in a child's report, onto the session's */
static int
account(stralloc *s)
{
  unsigned long v[8];
  size_t i = 0, k, e = byte_chr(s->s, s->len, '\n');

  if (e == s->len) return -1;
  for (k = 0; k < 8; ++k) {
    i += scan_ulongb(s->s + i, e - i, &v[k]);
    if (k < 7 && (i >= e || s->s[i] != ' ')) return -1;
    ++i;
  }
  requests += v[1];
  cost += v[2];
  if (v[3]) costgap = 1;
  total.in += v[4];
  total.cread += v[5];
  total.cwrite += v[6];
  total.out += v[7];
  byte_copy(s->s, s->len - e - 1, s->s + e + 1);
  s->len -= e + 1;
  return (int)v[0];
}

/*
 * runs the k calls t[i] with inputs in[i] (n[i] bytes) at once, each in
 * a process of its own, and puts their outputs in out[i] and whether
 * each went well in ok[i].
 */
void
parallel(const struct tool **t, const char **in, const size_t *n, size_t k,
    stralloc *out, int *ok)
{
  struct pollfd p[MAXPAR];
  pid_t pid[MAXPAR];
  struct usage u0;
  unsigned long long c0;
  unsigned long r0;
  size_t base, i, m, open_;
  char b[8192], lk[] = "/tmp/kraft-lock.XXXXXX";
  ssize_t r;
  int fd[2];

  flush();
  if (lockfd == -1) {
    /* one at a time, for the terminal, /undo and MCP's pipes */
    if ((lockfd = mkstemp(lk)) != -1) {
      unlink(lk);
      fcntl(lockfd, F_SETFD, FD_CLOEXEC);
    }
  }
  for (base = 0; base < k; base += MAXPAR) {
    m = k - base < MAXPAR ? k - base : MAXPAR;
    for (i = 0; i < m; ++i) {
      if (!stralloc_copys(&out[base + i], "")) nomem();
      ok[base + i] = 0;
      p[i].fd = -1;
      pid[i] = -1;
      if (pipe(fd) == -1) {
        if (!stralloc_copys(&out[base + i], "unable to run it: no pipe")) nomem();
        continue;
      }
      if ((pid[i] = fork()) == -1) {
        close(fd[0]);
        close(fd[1]);
        if (!stralloc_copys(&out[base + i], "unable to run it: no fork")) nomem();
        continue;
      }
      if (pid[i] == 0) {
        stralloc o = STRALLOC_ZERO;
        int good;

        close(fd[0]);
        inchild = 1;
        u0 = total;
        c0 = cost;
        r0 = requests;
        good = t[base + i]->run(in[base + i], n[base + i], &o);
        flush();
        report(fd[1], good, &u0, c0, r0, &o);
        _exit(0);  /* not exit(): the atexit handlers are the parent's */
      }
      close(fd[1]);
      p[i].fd = fd[0];
      p[i].events = POLLIN;
    }
    /* read them all as they come, so none blocks on a full pipe */
    for (;;) {
      for (i = open_ = 0; i < m; ++i) open_ += p[i].fd != -1;
      if (!open_) break;
      if (poll(p, (nfds_t)m, -1) == -1) continue;  /* EINTR: keep reading */
      for (i = 0; i < m; ++i) {
        if (p[i].fd == -1 || !(p[i].revents & (POLLIN | POLLHUP | POLLERR)))
          continue;
        r = read(p[i].fd, b, sizeof b);
        if (r == -1 && errno == EINTR) continue;
        if (r <= 0) {
          close(p[i].fd);
          p[i].fd = -1;
          continue;
        }
        if (!stralloc_catb(&out[base + i], b, (size_t)r)) nomem();
      }
      for (i = 0; i < m; ++i) if (p[i].fd == -1) p[i].events = 0;
    }
    for (i = 0; i < m; ++i) {
      if (pid[i] == -1) continue;
      while (waitpid(pid[i], 0, 0) == -1 && errno == EINTR) ;
      if ((ok[base + i] = account(&out[base + i])) < 0) {
        ok[base + i] = 0;
        if (!stralloc_copys(&out[base + i], "the call failed: its process "
            "ended without a result"))
          nomem();
      }
    }
  }
  /* the children may have saved files for /undo */
  while (undodir.len) {
    if (!stralloc_copy(&tmp, &undodir) || !stralloc_cats(&tmp, "/") ||
        !stralloc_catulong(&tmp, undoseq + 1) || !stralloc_0(&tmp))
      nomem();
    if (access(tmp.s, F_OK) == -1) break;
    ++undoseq;
  }
}
