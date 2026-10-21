#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "byte.h"
#include "http.h"
#include "oauth.h"
#include "str.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"
#include "url.h"

#define CONNECTSECS 30
#define MAXBODY (1024 * 1024)

/*
 * makes one request: method to url (http or https), with extra
 * header lines hdrs (each ending in CRLF, or 0) and body (n bytes, or
 * 0 for none).  returns 1 if the server answered, whatever its
 * status: h then holds the status and headers, and out the body (at
 * most a megabyte).  returns 0 with the reason in why otherwise.
 */
int
http_fetch(const char *method, const char *url, const char *hdrs,
    const char *body, size_t n, struct http *h, stralloc *out, stralloc *why)
{
  static struct url u;
  static stralloc host, path;
  char b[8192];
  ssize_t r;
  int fd;

  if (!url_parse(&u, url)) {
    stralloc_copys(why, "not a URL: ");
    stralloc_cats(why, url);
    return 0;
  }
  if (!stralloc_copys(&host, "")) goto nomem;
  if (byte_chr(u.host.s, u.host.len, ':') < u.host.len - 1) {
    if (!stralloc_cats(&host, "[") || !stralloc_cats(&host, u.host.s) ||
        !stralloc_cats(&host, "]"))
      goto nomem;
  } else if (!stralloc_cats(&host, u.host.s)) goto nomem;
  if (str_diff(u.port.s, u.tls ? "443" : "80") &&
      (!stralloc_cats(&host, ":") || !stralloc_cats(&host, u.port.s)))
    goto nomem;
  if (!stralloc_0(&host)) goto nomem;
  /* url_parse() drops a trailing slash; the query is in the path */
  if (!stralloc_copys(&path, u.path.len > 1 ? u.path.s : "/") ||
      !stralloc_0(&path))
    goto nomem;

  if ((fd = tcp_connect(u.host.s, u.port.s, CONNECTSECS)) == -1) {
    stralloc_copys(why, "unable to connect to ");
    stralloc_cats(why, host.s);
    stralloc_cats(why, ": ");
    stralloc_cats(why, strerror(errno));
    return 0;
  }
  if (u.tls && (fd = tls_connect(fd, u.host.s, why)) == -1) return 0;
  if (!http_request(fd, method, host.s, path.s, hdrs, body, n) ||
      http_response(h, fd) <= 0) {
    close(fd);
    stralloc_copys(why, "no answer from ");
    stralloc_cats(why, host.s);
    return 0;
  }
  if (!stralloc_copys(out, "")) goto nomem;
  while ((r = http_get(h, b, sizeof b)) > 0)
    if (out->len < MAXBODY && !stralloc_catb(out, b, (size_t)r)) {
      close(fd);
      goto nomem;
    }
  close(fd);
  return 1;
nomem:
  stralloc_copys(why, "out of memory");
  return 0;
}
