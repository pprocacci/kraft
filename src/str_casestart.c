#include "str.h"

static int
lower(int c)
{
  return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

/* 1 if s begins with t, ignoring ASCII case */
int
str_casestart(const char *s, const char *t)
{
  for (; *t; ++s, ++t)
    if (lower((unsigned char)*s) != lower((unsigned char)*t)) return 0;
  return 1;
}
