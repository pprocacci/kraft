#include <errno.h>
#include <unistd.h>
#include "mcp.h"
#include "stralloc.h"

/*
 * sends message s (n bytes, without a newline) to m, expecting no
 * answer: a notification, or an answer to the server; 1 if it went
 */
int
mcp_send(struct mcp *m, const char *s, size_t n)
{
  stralloc why = STRALLOC_ZERO;
  ssize_t w;
  int ok;

  if (!m->up) return 0;
  if (m->http) {
    ok = mcp_post(m, s, n, 0, 0, &why, 0);
    stralloc_free(&why);
    return ok;
  }
  while (n) {
    w = write(m->to, s, n);
    if (w == -1 && errno == EINTR) continue;
    if (w <= 0) return 0;
    s += w;
    n -= (size_t)w;
  }
  while ((w = write(m->to, "\n", 1)) == -1 && errno == EINTR) ;
  return w == 1;
}
