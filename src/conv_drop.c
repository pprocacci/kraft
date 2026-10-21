#include "alloc.h"
#include "conv.h"
#include "stralloc.h"

/* removes the last message, if any */
void
conv_drop(struct conv *c)
{
  struct msg *m;
  size_t i;

  if (!c->n) return;
  m = &c->m[--c->n];
  for (i = 0; i < m->n; ++i) {
    stralloc_free(&m->b[i].id);
    stralloc_free(&m->b[i].name);
    stralloc_free(&m->b[i].text);
    stralloc_free(&m->b[i].aux);
  }
  alloc_free(m->b);
  m->b = 0;
  m->n = m->a = 0;
}
