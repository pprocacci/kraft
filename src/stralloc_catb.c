#include "byte.h"
#include "stralloc.h"

int
stralloc_catb(stralloc *sa, const char *s, size_t n)
{
  if (!stralloc_readyplus(sa, n + 1)) return 0;
  byte_copy(sa->s + sa->len, n, s);
  sa->len += n;
  sa->s[sa->len] = 'Z';
  return 1;
}
