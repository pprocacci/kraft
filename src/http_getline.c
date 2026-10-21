#include <errno.h>
#include "buffer.h"
#include "http.h"
#include "stralloc.h"

#define MAXLINE 65536

/*
 * reads one raw line from the connection into line, without the
 * trailing CRLF and NUL-terminated (the NUL is not counted in len).
 * returns 1 on success, 0 at end of file, -1 on error.
 */
int
http_getline(struct http *h, stralloc *line)
{
  char c;
  ssize_t r;

  if (!stralloc_copys(line, "")) return -1;
  for (;;) {
    r = buffer_get(&h->b, &c, 1);
    if (r < 0) return -1;
    if (r == 0) {
      if (!line->len) return 0;
      break;
    }
    if (c == '\n') break;
    if (line->len >= MAXLINE) { errno = EPROTO; return -1; }
    if (!stralloc_append(line, c)) return -1;
  }
  if (line->len && line->s[line->len - 1] == '\r') --line->len;
  if (!stralloc_0(line)) return -1;
  --line->len;
  return 1;
}
