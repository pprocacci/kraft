#include "byte.h"

void
byte_copy(void *to, size_t n, const void *from)
{
  char *t = to;
  const char *f = from;

  while (n--) *t++ = *f++;
}
