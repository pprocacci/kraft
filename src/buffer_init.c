#include "buffer.h"

void
buffer_init(buffer *b, ssize_t (*op)(int, char *, size_t), int fd,
    char *x, size_t a)
{
  b->x = x;
  b->a = a;
  b->p = 0;
  b->n = 0;
  b->fd = fd;
  b->op = op;
}
