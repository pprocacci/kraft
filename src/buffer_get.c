#include "byte.h"
#include "buffer.h"

/* reads up to n bytes; returns count, 0 at end of file, -1 on error */
ssize_t
buffer_get(buffer *b, char *s, size_t n)
{
  ssize_t r;
  size_t k;

  if (!n) return 0;
  if (b->p == b->n) {
    if (n >= b->a) return b->op(b->fd, s, n);
    r = b->op(b->fd, b->x, b->a);
    if (r <= 0) return r;
    b->p = 0;
    b->n = (size_t)r;
  }
  k = b->n - b->p;
  if (k > n) k = n;
  byte_copy(s, k, b->x + b->p);
  b->p += k;
  return (ssize_t)k;
}
