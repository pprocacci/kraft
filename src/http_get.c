#include <errno.h>
#include "buffer.h"
#include "http.h"
#include "scan.h"
#include "stralloc.h"

static int
nextchunk(struct http *h)
{
  stralloc line = STRALLOC_ZERO;
  int r;

  /* the CRLF that ends the previous chunk */
  if (h->started) {
    r = http_getline(h, &line);
    if (r <= 0 || line.len) goto bad;
  }
  h->started = 1;
  r = http_getline(h, &line);
  if (r <= 0) goto bad;
  if (!scan_xlong(line.s, &h->left)) goto bad;
  if (!h->left) {
    /* trailers, up to an empty line */
    do {
      r = http_getline(h, &line);
      if (r < 0) goto bad;
    } while (r && line.len);
    h->eof = 1;
  }
  stralloc_free(&line);
  return 1;
bad:
  stralloc_free(&line);
  if (r >= 0) errno = EPROTO;
  return 0;
}

/*
 * reads up to n bytes of decoded body.  returns the count, 0 when the
 * body is complete, -1 on error or truncation.
 */
ssize_t
http_get(struct http *h, char *s, size_t n)
{
  ssize_t r;

  if (h->eof || !n) return 0;
  if (h->chunked) {
    if (!h->left) {
      if (!nextchunk(h)) return -1;
      if (h->eof) return 0;
    }
  } else if (h->haslen && !h->left) {
    h->eof = 1;
    return 0;
  }
  if ((h->chunked || h->haslen) && n > h->left) n = h->left;
  r = buffer_get(&h->b, s, n);
  if (r < 0) return -1;
  if (r == 0) {
    h->eof = 1;
    if (h->chunked || h->haslen) { errno = EPROTO; return -1; }
    return 0;
  }
  if (h->chunked || h->haslen) h->left -= (unsigned long)r;
  return r;
}
