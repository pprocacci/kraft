#include "byte.h"
#include "buffer.h"

/* writes out everything pending; 0 on success, -1 on error */
int
buffer_flush(buffer *b)
{
  size_t done = 0;
  ssize_t w;

  while (done < b->p) {
    w = b->op(b->fd, b->x + done, b->p - done);
    if (w <= 0) {
      /* keep what was not written */
      byte_copy(b->x, b->p - done, b->x + done);
      b->p -= done;
      return -1;
    }
    done += (size_t)w;
  }
  b->p = 0;
  return 0;
}
