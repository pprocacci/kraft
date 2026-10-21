#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "http.h"
#include "mcp.h"
#include "sse.h"
#include "str.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"

#define CONNECTSECS 30
#define MAXBODY (64 * 1024 * 1024)

/*
 * POSTs message s (n bytes) to HTTP server m.  for a request (id
 * given, idn bytes) it waits for the answer, in a JSON body or an
 * event stream, as mcp_take() says; for anything else, a 2xx is 1.
 * returns 1 or 0 as mcp_take(), with the reason in out.  everything
 * is local: answering the server's ping mid-stream comes back here.
 */
int
mcp_post(struct mcp *m, const char *s, size_t n, const char *id, size_t idn,
    stralloc *out, volatile sig_atomic_t *stop)
{
  struct http h;
  struct sse sse = SSE_ZERO;
  stralloc hdrs = STRALLOC_ZERO, ev = STRALLOC_ZERO, data = STRALLOC_ZERO;
  char b[8192];
  ssize_t r;
  int fd = -1, got = 0;

  memset(&h, 0, sizeof h);
  if (!stralloc_copys(&hdrs, "Content-Type: application/json\r\n"
      "Accept: application/json, text/event-stream\r\n") ||
      (m->id > 1 && !stralloc_cats(&hdrs, "MCP-Protocol-Version: 2025-06-18\r\n")) ||
      (m->session.len && (!stralloc_cats(&hdrs, "Mcp-Session-Id: ") ||
      !stralloc_cat(&hdrs, &m->session) || !stralloc_cats(&hdrs, "\r\n"))) ||
      (m->token.len && (!stralloc_cats(&hdrs, "Authorization: Bearer ") ||
      !stralloc_cat(&hdrs, &m->token) || !stralloc_cats(&hdrs, "\r\n"))) ||
      !stralloc_cat(&hdrs, &m->hdrs) || !stralloc_0(&hdrs))
    goto nomem;
  m->status = 0;
  if ((fd = tcp_connect(m->u.host.s, m->u.port.s, CONNECTSECS)) == -1) {
    stralloc_copys(out, "unable to connect: ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  if (m->u.tls && (fd = tls_connect(fd, m->u.host.s, &data)) == -1) {
    stralloc_copys(out, "TLS: ");
    stralloc_cat(out, &data);
    goto out;
  }
  if (!http_request(fd, "POST", m->hosthdr.s, m->u.path.len > 1 ?
      m->u.path.s : "/", hdrs.s, s, n) || http_response(&h, fd) <= 0) {
    stralloc_copys(out, stop && *stop ? "interrupted" :
        "no answer from the server");
    goto out;
  }
  if (h.session.len > 1 && !stralloc_copyb(&m->session, h.session.s,
      h.session.len - 1))
    goto nomem;

  m->status = h.status;
  if (!stralloc_copy(&m->auth, &h.auth)) goto nomem;
  if (h.status / 100 != 2) {
    stralloc_copys(out, "HTTP ");
    stralloc_catulong(out, (unsigned long)h.status);
    if (h.status == 401)
      stralloc_cats(out, m->token.len ? " (the login has run out: /mcp login)" :
          " (it wants you to log in: /mcp login)");
    else if (h.status == 403)
      stralloc_cats(out, " (not allowed)");
    else if (h.status == 404 && m->session.len)
      stralloc_cats(out, " (the server has forgotten the session)");
    goto out;
  }
  if (!id) {
    while (http_get(&h, b, sizeof b) > 0) ;
    got = 1;
    goto out;
  }

  got = -1;
  if (str_start(h.ctype.s, "text/event-stream")) {
    while (got < 0 && !(stop && *stop) && sse_get(&sse, &h, &ev, &data) > 0)
      got = mcp_take(m, data.s, data.len, id, idn, out);
  } else {
    while ((r = http_get(&h, b, sizeof b)) > 0)
      if (data.len > MAXBODY || !stralloc_catb(&data, b, (size_t)r))
        goto nomem;
    got = mcp_take(m, data.s, data.len, id, idn, out);
  }
  if (got < 0) {
    stralloc_copys(out, stop && *stop ? "interrupted" :
        "the server did not answer the request");
    got = 0;
  }
  goto out;
nomem:
  stralloc_copys(out, "out of memory");
  got = 0;
out:
  if (fd != -1) close(fd);
  stralloc_free(&hdrs);
  stralloc_free(&ev);
  stralloc_free(&data);
  stralloc_free(&sse.line);
  stralloc_free(&h.loc);
  stralloc_free(&h.ctype);
  stralloc_free(&h.session);
  stralloc_free(&h.auth);
  return got;
}
