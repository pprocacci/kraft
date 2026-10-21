#include <errno.h>
#include <stdint.h>
#include "alloc.h"
#include "stralloc.h"

/* makes room for at least n bytes in sa */
int
stralloc_ready(stralloc *sa, size_t n)
{
  size_t a;
  void *p;

  if (sa->s && sa->a >= n) return 1;
  if (n > SIZE_MAX - 30 - (n >> 3)) { errno = ENOMEM; return 0; }
  a = n + 30 + (n >> 3);
  if (!sa->s) sa->len = 0;
  p = sa->s;
  if (!alloc_re(&p, a)) return 0;
  sa->s = p;
  sa->a = a;
  return 1;
}
