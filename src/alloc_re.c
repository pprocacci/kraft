#include <errno.h>
#include <stdlib.h>
#include "alloc.h"

int
alloc_re(void **x, size_t n)
{
  void *y;

  y = realloc(*x, n ? n : 1);
  if (!y) { errno = ENOMEM; return 0; }
  *x = y;
  return 1;
}
