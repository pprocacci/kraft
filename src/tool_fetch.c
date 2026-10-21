#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "byte.h"
#include "html.h"
#include "http.h"
#include "str.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"
#include "tool.h"
#include "url.h"

#define MAXBODY (2 * 1024 * 1024)  /* bytes read from the server */
#define MAXTEXT 20000              /* bytes given to the model, by default */
#define REDIRECTS 5

static struct http h;  /* large; one fetch at a time */

static int
fail(stralloc *out, const char *a, const char *b, const char *c)
{
  stralloc_copys(out, a);
  if (b) stralloc_cats(out, b);
  if (c) stralloc_cats(out, c);
  return 0;
}

/* the part of an http(s) URL after the host and port, with no fragment */
static const char *
rest(const char *u, size_t *n)
{
  const char *s = u + (str_start(u, "https://") ? 8 : 7);
  size_t k;

  while (*s && *s != '/' && *s != '?' && *s != '#') ++s;
  k = byte_chr(s, str_len(s), '#');
  *n = k;
  return s;
}

/* makes cur the target of Location loc */
static int
follow(stralloc *cur, const char *loc)
{
  stralloc t = STRALLOC_ZERO;
  const char *r;
  size_t n, k;
  int ok = 0;

  if (str_start(loc, "http://") || str_start(loc, "https://")) {
    if (!stralloc_copys(&t, loc)) goto out;
  } else if (str_start(loc, "//")) {
    if (!stralloc_copys(&t, str_start(cur->s, "https:") ? "https:" : "http:") ||
        !stralloc_cats(&t, loc))
      goto out;
  } else {
    r = rest(cur->s, &n);
    k = (size_t)(r - cur->s);  /* scheme and host */
    if (!stralloc_copyb(&t, cur->s, k)) goto out;
    if (*loc == '/') {
      if (!stralloc_cats(&t, loc)) goto out;
    } else {
      /* relative to the current directory */
      while (n && r[n - 1] != '/') --n;
      if (!n && !stralloc_cats(&t, "/")) goto out;
      if (!stralloc_catb(&t, r, n) || !stralloc_cats(&t, loc)) goto out;
    }
  }
  if (!stralloc_0(&t) || !stralloc_copy(cur, &t)) goto out;
  ok = 1;
out:
  stralloc_free(&t);
  return ok;
}

/*
 * web_fetch: url, optional max_length.  fetches the page with GET,
 * following redirects, and returns its text: HTML turned into plain
 * text, other text types as they are.
 */
int
tool_fetch(const char *in, size_t n, stralloc *out)
{
  static struct url u;
  stralloc cur = STRALLOC_ZERO, host = STRALLOC_ZERO, path = STRALLOC_ZERO;
  stralloc body = STRALLOC_ZERO, why = STRALLOC_ZERO, txt = STRALLOC_ZERO;
  struct timeval tv = { 30, 0 };
  unsigned long max = MAXTEXT;
  const char *r, *ct;
  char buf[8192];
  size_t rn;
  ssize_t got;
  int fd = -1, hops, ok = 0;

  if (!tool_arg(in, n, "url", &cur)) return fail(out, "url is required", 0, 0);
  tool_argnum(in, n, "max_length", &max);
  if (!max || max > 200000) max = MAXTEXT;

  for (hops = 0;; ++hops) {
    if (!url_parse(&u, cur.s)) {
      fail(out, "not an http or https URL: ", cur.s, 0);
      goto out;
    }
    r = rest(cur.s, &rn);
    if (!stralloc_copyb(&path, rn ? r : "/", rn ? rn : 1) ||
        !stralloc_0(&path))
      goto nomem;
    if (*path.s != '/') {  /* a bare query: host?x=y */
      if (!stralloc_copys(&txt, "/") || !stralloc_cats(&txt, path.s) ||
          !stralloc_0(&txt) || !stralloc_copy(&path, &txt))
        goto nomem;
    }
    if (byte_chr(u.host.s, u.host.len, ':') < u.host.len - 1) {
      if (!stralloc_copys(&host, "[") || !stralloc_cats(&host, u.host.s) ||
          !stralloc_cats(&host, "]"))
        goto nomem;
    } else if (!stralloc_copys(&host, u.host.s)) goto nomem;
    if (str_diff(u.port.s, u.tls ? "443" : "80") &&
        (!stralloc_cats(&host, ":") || !stralloc_cats(&host, u.port.s)))
      goto nomem;
    if (!stralloc_0(&host)) goto nomem;

    fd = tcp_connect(u.host.s, u.port.s, 15);
    if (fd == -1) {
      fail(out, "unable to connect to ", u.host.s, ": ");
      stralloc_cats(out, strerror(errno));
      goto out;
    }
    if (u.tls && (fd = tls_connect(fd, u.host.s, &why)) == -1) {
      fail(out, "unable to start TLS with ", u.host.s, ": ");
      stralloc_cats(out, why.len ? why.s : strerror(errno));
      goto out;
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (!http_request(fd, "GET", host.s, path.s,
        "Accept: text/html, text/plain, application/json, */*;q=0.5\r\n", 0, 0)
        || http_response(&h, fd) <= 0) {
      fail(out, "no valid HTTP response from ", u.host.s, 0);
      goto out;
    }
    if ((h.status == 301 || h.status == 302 || h.status == 303 ||
        h.status == 307 || h.status == 308) && h.loc.len > 1) {
      close(fd);
      fd = -1;
      if (hops == REDIRECTS) {
        fail(out, "too many redirects", 0, 0);
        goto out;
      }
      if (!follow(&cur, h.loc.s)) goto nomem;
      continue;
    }
    break;
  }

  while (body.len < MAXBODY && (got = http_get(&h, buf, sizeof buf)) > 0)
    if (!stralloc_catb(&body, buf, (size_t)got)) goto nomem;
  close(fd);
  fd = -1;

  if (h.status != 200) {
    stralloc_copys(out, "HTTP ");
    stralloc_catulong(out, (unsigned long)h.status);
    stralloc_cats(out, " from ");
    stralloc_cats(out, cur.s);
    goto out;
  }
  ct = h.ctype.s;
  if (str_casestart(ct, "text/html") || str_casestart(ct, "application/xhtml")) {
    if (!html_text(&txt, body.s, body.len)) goto nomem;
  } else if (!*ct || str_casestart(ct, "text/") ||
      str_casestart(ct, "application/json") ||
      str_casestart(ct, "application/xml") ||
      str_casestart(ct, "application/javascript")) {
    if (byte_chr(body.s, body.len, '\0') < body.len) {
      fail(out, "refused: the content is binary", 0, 0);
      goto out;
    }
    if (!stralloc_copy(&txt, &body)) goto nomem;
  } else {
    fail(out, "refused: ", ct, " is not text");
    goto out;
  }

  if (!stralloc_copys(out, "URL: ") || !stralloc_cats(out, cur.s) ||
      !stralloc_cats(out, "\n\n") ||
      !stralloc_catb(out, txt.s, txt.len > max ? max : txt.len))
    goto nomem;
  if (txt.len > max) {
    stralloc_cats(out, "\n[");
    stralloc_catulong(out, (unsigned long)(txt.len - max));
    stralloc_cats(out, " more bytes not shown; raise max_length to see more]");
  }
  if (!txt.len) stralloc_cats(out, "(no text)");
  ok = 1;
  goto out;
nomem:
  fail(out, "out of memory", 0, 0);
out:
  if (fd != -1) close(fd);
  stralloc_free(&cur);
  stralloc_free(&host);
  stralloc_free(&path);
  stralloc_free(&body);
  stralloc_free(&why);
  stralloc_free(&txt);
  return ok;
}
