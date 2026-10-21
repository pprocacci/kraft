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

/* "server's" error message, or its code */
static void
failed(const char *e, size_t n, stralloc *out)
{
  const char *v;
  size_t vn;

  stralloc_copys(out, "");
  if (json_get(e, n, "message", &v, &vn) && json_unstr(out, v, vn) && out->len)
    return;
  stralloc_copys(out, "error ");
  if (json_get(e, n, "code", &v, &vn)) stralloc_catb(out, v, vn);
}

/* answers a request from the server: a ping, or "not supported" */
static void
answer(struct mcp *m, const char *s, size_t n, const char *id, size_t idn)
{
  stralloc r = STRALLOC_ZERO;
  const char *v;
  size_t vn;
  int ping = json_get(s, n, "method", &v, &vn) && json_is(v, vn, "ping");

  if (stralloc_copys(&r, "{\"jsonrpc\":\"2.0\",\"id\":") &&
      stralloc_catb(&r, id, idn) &&
      stralloc_cats(&r, ping ? ",\"result\":{}}" : ",\"error\":{\"code\":"
          "-32601,\"message\":\"kraft does not support that\"}}"))
    mcp_send(m, r.s, r.len);
  stralloc_free(&r);
}

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
 * to secs seconds for the answer, or until *stop is set.  returns 1
 * with the result's JSON in out, or 0 with the reason.  messages
 * that are not the answer are dealt with or skipped.
 */
int
mcp_rpc(struct mcp *m, const char *method, const char *params, stralloc *out,
    unsigned long secs, volatile sig_atomic_t *stop)
{
  char idbuf[FMT_ULONG], b[8192];
  struct pollfd p;
  const char *v, *id;
  size_t vn, idn, i, e;
  time_t end = time(0) + (time_t)secs;
  ssize_t r;
  int k, ok;

  if (!m->pid) {
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
  if (!mcp_send(m, m->line.s, m->line.len)) {
    mcp_stop(m);
    stralloc_copys(out, "the server has gone away");
    return 0;
  }
  k = (int)fmt_ulong(idbuf, m->id);

  for (;;) {
    /* whole lines already read */
    while ((i = byte_chr(m->buf.s, m->buf.len, '\n')) < m->buf.len) {
      e = i;
      if (e && m->buf.s[e - 1] == '\r') --e;
      if (!stralloc_copyb(&m->line, m->buf.s, e)) goto nomem;
      byte_copy(m->buf.s, m->buf.len - i - 1, m->buf.s + i + 1);
      m->buf.len -= i + 1;
      if (!json_object(m->line.s, m->line.len)) continue;
      if (!json_get(m->line.s, m->line.len, "id", &id, &idn)) continue;
      if (json_get(m->line.s, m->line.len, "method", &v, &vn)) {
        answer(m, m->line.s, m->line.len, id, idn);
        continue;
      }
      if (idn != (size_t)k || byte_diff(id, idn, idbuf)) continue;
      if (json_get(m->line.s, m->line.len, "result", &v, &vn)) {
        ok = stralloc_copyb(out, v, vn);
        if (!ok) goto nomem;
        return 1;
      }
      if (json_get(m->line.s, m->line.len, "error", &v, &vn))
        failed(v, vn, out);
      else
        stralloc_copys(out, "the answer had neither result nor error");
      return 0;
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
