#include "byte.h"
#include "buffer.h"

int
buffer_put(buffer *b, const char *s, size_t n)
{
  size_t k;

  while (n) {
    if (b->p == b->a && buffer_flush(b) == -1) return -1;
    k = b->a - b->p;
    if (k > n) k = n;
    byte_copy(b->x + b->p, k, s);
    b->p += k;
    s += k;
    n -= k;
  }
  return 0;
}
