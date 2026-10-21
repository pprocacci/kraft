#include <errno.h>
#include "buffer.h"
#include "http.h"
#include "stralloc.h"

/*
 * sends one HTTP/1.1 request on fd.  hdrs holds extra header lines,
 * each ending in CRLF (may be null).  returns 1 on success, 0 on error.
 */
int
http_request(int fd, const char *method, const char *host,
    const char *path, const char *hdrs, const char *body, size_t len)
{
  stralloc r = STRALLOC_ZERO;
  size_t done = 0;
  ssize_t w;
  int ok = 0;

  if (!stralloc_copys(&r, method)) goto out;
  if (!stralloc_cats(&r, " ")) goto out;
  if (!stralloc_cats(&r, *path ? path : "/")) goto out;
  if (!stralloc_cats(&r, " HTTP/1.1\r\nHost: ")) goto out;
  if (!stralloc_cats(&r, host)) goto out;
  if (!stralloc_cats(&r, "\r\nUser-Agent: kraft\r\nConnection: close\r\n"))
    goto out;
  if (hdrs && !stralloc_cats(&r, hdrs)) goto out;
  if (body) {
    if (!stralloc_cats(&r, "Content-Length: ")) goto out;
    if (!stralloc_catulong(&r, len)) goto out;
    if (!stralloc_cats(&r, "\r\n")) goto out;
  }
  if (!stralloc_cats(&r, "\r\n")) goto out;
  if (body && !stralloc_catb(&r, body, len)) goto out;

  while (done < r.len) {
    w = buffer_unixwrite(fd, r.s + done, r.len - done);
    if (w <= 0) { if (!w) errno = EPIPE; goto out; }
    done += (size_t)w;
  }
  ok = 1;
out:
  stralloc_free(&r);
  return ok;
}
