#include <errno.h>
#include <stdint.h>
#include "alloc.h"
#include "conv.h"

/* appends an empty block of the given type; 0 if out of memory */
struct block *
conv_block(struct msg *m, int type)
{
  static const struct block zero;
  struct block *b;
  void *p;
  size_t a;

  if (m->n == m->a) {
    a = m->a ? m->a * 2 : 4;
    if (a > SIZE_MAX / sizeof *b) { errno = ENOMEM; return 0; }
    p = m->b;
    if (!alloc_re(&p, a * sizeof *b)) return 0;
    m->b = p;
    m->a = a;
  }
  b = &m->b[m->n++];
  *b = zero;
  b->type = type;
  b->idx = -1;
  return b;
}
