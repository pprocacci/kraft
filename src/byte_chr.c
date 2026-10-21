#include "byte.h"

size_t
byte_chr(const char *s, size_t n, int c)
{
  size_t i;

  for (i = 0; i < n; ++i)
    if (s[i] == (char)c) break;
  return i;
}
