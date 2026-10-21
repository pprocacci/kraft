#include <errno.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "byte.h"
#include "fmt.h"
#include "json.h"
#include "mcp.h"
#include "stralloc.h"

#define MAXLINE (64 * 1024 * 1024)

/* tells the server we have stopped waiting for request id */
static void
cancel(struct mcp *m, unsigned long id)
{
  stralloc r = STRALLOC_ZERO;

  if (stralloc_copys(&r, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/"
      "cancelled\",\"params\":{\"requestId\":") && stralloc_catulong(&r, id) &&
      stralloc_cats(&r, ",\"reason\":\"the user interrupted\"}}"))
    mcp_send(m, r.s, r.len);
  stralloc_free(&r);
}

/*
 * calls method on m with params (a JSON object, or 0), and waits up
 * to secs seconds (on stdio) for the answer, or until *stop is set.  returns 1
 * with the result's JSON in out, or 0 with the reason.  messages
 * that are not the answer are dealt with or skipped.
 */
int
mcp_rpc(struct mcp *m, const char *method, const char *params, stralloc *out,
    unsigned long secs, volatile sig_atomic_t *stop)
{
  char idbuf[FMT_ULONG], b[8192];
  struct pollfd p;
  size_t i, e;
  time_t end = time(0) + (time_t)secs;
  ssize_t r;
  int k, got;

  if (!m->up) {
    stralloc_copys(out, "the server is not running");
    return 0;
  }
  ++m->id;
  if (!stralloc_copys(&m->line, "{\"jsonrpc\":\"2.0\",\"id\":") ||
      !stralloc_catulong(&m->line, m->id) ||
      !stralloc_cats(&m->line, ",\"method\":") ||
      !json_str(&m->line, method, strlen(method)) ||
      (params && (!stralloc_cats(&m->line, ",\"params\":") ||
      !stralloc_cats(&m->line, params))) || !stralloc_cats(&m->line, "}")) {
    stralloc_copys(out, "out of memory");
    return 0;
  }
  k = (int)fmt_ulong(idbuf, m->id);
  if (m->http) {
    got = mcp_post(m, m->line.s, m->line.len, idbuf, (size_t)k, out, stop);
    if (!got && stop && *stop) cancel(m, m->id);
    return got;
  }
  if (!mcp_send(m, m->line.s, m->line.len)) {
    mcp_stop(m);
    stralloc_copys(out, "the server has gone away");
    return 0;
  }

  for (;;) {
    /* whole lines already read */
    while ((i = byte_chr(m->buf.s, m->buf.len, '\n')) < m->buf.len) {
      e = i;
      if (e && m->buf.s[e - 1] == '\r') --e;
      if (!stralloc_copyb(&m->line, m->buf.s, e)) goto nomem;
      byte_copy(m->buf.s, m->buf.len - i - 1, m->buf.s + i + 1);
      m->buf.len -= i + 1;
      if ((got = mcp_take(m, m->line.s, m->line.len, idbuf, (size_t)k,
          out)) >= 0)
        return got;
    }
    if (m->buf.len > MAXLINE) {
      mcp_stop(m);
      stralloc_copys(out, "the server sent an over-long message");
      return 0;
    }
    if (stop && *stop) {
      cancel(m, m->id);
      stralloc_copys(out, "interrupted");
      return 0;
    }
    if (time(0) >= end) {
      cancel(m, m->id);
      stralloc_copys(out, "no answer from the server in time");
      return 0;
    }
    p.fd = m->from;
    p.events = POLLIN;
    if (poll(&p, 1, 1000) <= 0) continue;  /* EINTR included */
    r = read(m->from, b, sizeof b);
    if (r == -1 && (errno == EINTR || errno == EAGAIN)) continue;
    if (r <= 0) {
      mcp_stop(m);
      stralloc_copys(out, "the server has exited");
      return 0;
    }
    if (!stralloc_catb(&m->buf, b, (size_t)r)) goto nomem;
  }
nomem:
  stralloc_copys(out, "out of memory");
  return 0;
}
