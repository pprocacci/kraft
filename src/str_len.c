#include "str.h"

size_t
str_len(const char *s)
{
  const char *t = s;

  while (*t) ++t;
  return (size_t)(t - s);
}
