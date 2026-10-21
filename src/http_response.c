#include <errno.h>
#include "buffer.h"
#include "http.h"
#include "scan.h"
#include "str.h"
#include "stralloc.h"

static const char *
value(const char *s)
{
  while (*s && *s != ':') ++s;
  if (*s) ++s;
  while (*s == ' ' || *s == '\t') ++s;
  return s;
}

static int
contains(const char *s, const char *t)
{
  for (; *s; ++s)
    if (str_casestart(s, t)) return 1;
  return 0;
}

/*
 * reads the status line and headers of a response on fd.
 * returns 1 on success, 0 on a malformed response, -1 on error.
 */
int
http_response(struct http *h, int fd)
{
  stralloc line = STRALLOC_ZERO;
  unsigned long u;
  int r;

  h->fd = fd;
  buffer_init(&h->b, buffer_unixread, fd, h->x, sizeof h->x);
  h->status = 0;
  h->chunked = 0;
  h->haslen = 0;
  h->started = 0;
  h->eof = 0;
  h->left = 0;
  if (!stralloc_copys(&h->loc, "") || !stralloc_0(&h->loc) ||
      !stralloc_copys(&h->ctype, "") || !stralloc_0(&h->ctype))
    return -1;

  r = http_getline(h, &line);
  if (r <= 0) goto out;
  r = 0;
  if (!str_start(line.s, "HTTP/1.")) goto out;
  if (line.len < 12 || line.s[8] != ' ') goto out;
  if (scan_ulong(line.s + 9, &u) != 3) goto out;
  h->status = (int)u;

  for (;;) {
    r = http_getline(h, &line);
    if (r < 0) goto out;
    if (r == 0) { r = 0; goto out; }
    if (!line.len) break;
    if (str_casestart(line.s, "transfer-encoding:")) {
      if (contains(value(line.s), "chunked")) h->chunked = 1;
    } else if (str_casestart(line.s, "content-length:")) {
      if (!scan_ulong(value(line.s), &h->left)) { r = 0; goto out; }
      h->haslen = 1;
    } else if (str_casestart(line.s, "location:")) {
      if (!stralloc_copys(&h->loc, value(line.s)) || !stralloc_0(&h->loc))
        { r = -1; goto out; }
    } else if (str_casestart(line.s, "content-type:")) {
      if (!stralloc_copys(&h->ctype, value(line.s)) || !stralloc_0(&h->ctype))
        { r = -1; goto out; }
    }
  }
  if (h->chunked) { h->haslen = 0; h->left = 0; }
  if (h->status == 204 || h->status == 304) h->eof = 1;
  r = 1;
out:
  if (r == 0) errno = EPROTO;
  stralloc_free(&line);
  return r;
}
