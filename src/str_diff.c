#include "str.h"

int
str_diff(const char *s, const char *t)
{
  for (; *s && *s == *t; ++s, ++t) ;
  return (int)(unsigned char)*s - (int)(unsigned char)*t;
}
