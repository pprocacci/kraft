#include "byte.h"
#include "stralloc.h"

int
stralloc_copyb(stralloc *sa, const char *s, size_t n)
{
  if (!stralloc_ready(sa, n + 1)) return 0;
  byte_copy(sa->s, n, s);
  sa->len = n;
  sa->s[n] = 'Z'; /* catches users who forget stralloc_0 */
  return 1;
}
