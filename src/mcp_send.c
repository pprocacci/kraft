#include <errno.h>
#include <unistd.h>
#include "mcp.h"

/* writes message s (n bytes, without its newline) to m; 1 if all went */
int
mcp_send(struct mcp *m, const char *s, size_t n)
{
  ssize_t w;

  if (!m->pid) return 0;
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
