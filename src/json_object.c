#include "json.h"

/* 1 if s is exactly one well-formed JSON object, give or take spaces */
int
json_object(const char *s, size_t n)
{
  size_t i = 0, e;

  while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
      s[i] == '\r'))
    ++i;
  if (i >= n || s[i] != '{') return 0;
  if (!(e = json_skip(s, n, i))) return 0;
  while (e < n && (s[e] == ' ' || s[e] == '\t' || s[e] == '\n' ||
      s[e] == '\r'))
    ++e;
  return e == n;
}
