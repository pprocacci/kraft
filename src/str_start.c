#include "str.h"

/* 1 if s begins with t */
int
str_start(const char *s, const char *t)
{
  for (; *t; ++s, ++t)
    if (*s != *t) return 0;
  return 1;
}
