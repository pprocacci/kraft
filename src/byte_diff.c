#include "byte.h"

int
byte_diff(const void *s, size_t n, const void *t)
{
  const unsigned char *a = s;
  const unsigned char *b = t;

  for (; n; --n, ++a, ++b)
    if (*a != *b) return (int)*a - (int)*b;
  return 0;
}
