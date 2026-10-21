#include "byte.h"
#include "json.h"
#include "str.h"

/* 1 if the JSON value v is the string s (without escapes) */
int
json_is(const char *v, size_t n, const char *s)
{
  size_t k = str_len(s);

  return n == k + 2 && v[0] == '"' && v[n - 1] == '"' &&
      !byte_diff(v + 1, k, s);
}
