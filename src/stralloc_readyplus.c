#include <errno.h>
#include <stdint.h>
#include "stralloc.h"

/* makes room for n more bytes after sa->len */
int
stralloc_readyplus(stralloc *sa, size_t n)
{
  if (!sa->s) sa->len = 0;
  if (n > SIZE_MAX - sa->len) { errno = ENOMEM; return 0; }
  return stralloc_ready(sa, sa->len + n);
}
