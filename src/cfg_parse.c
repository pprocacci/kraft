#include "byte.h"
#include "cfg.h"
#include "stralloc.h"

static int
space(char c)
{
  return c == ' ' || c == '\t' || c == '\r';
}

/*
 * calls fn(arg, key, value, line) for each setting in s, with key and
 * value NUL-terminated and trimmed.  returns 0 if all went well, or
 * the number of the line where fn failed (or memory ran out).
 */
unsigned long
cfg_parse(const char *s, size_t n,
    int (*fn)(void *, const char *, const char *, unsigned long), void *arg)
{
  stralloc k = STRALLOC_ZERO, v = STRALLOC_ZERO;
  unsigned long line = 0, bad = 0;
  size_t i, e, a, b, j;

  for (i = 0; i < n && !bad; i = e + 1) {
    e = i + byte_chr(s + i, n - i, '\n');
    ++line;
    for (a = i; a < e && space(s[a]); ++a) ;
    for (b = e; b > a && space(s[b - 1]); --b) ;
    if (a == b || s[a] == '#') continue;
    for (j = a; j < b && !space(s[j]); ++j) ;
    if (!stralloc_copyb(&k, s + a, j - a) || !stralloc_0(&k)) { bad = line; break; }
    while (j < b && space(s[j])) ++j;
    if (!stralloc_copyb(&v, s + j, b - j) || !stralloc_0(&v)) { bad = line; break; }
    if (!fn(arg, k.s, v.s, line)) bad = line;
  }
  stralloc_free(&k);
  stralloc_free(&v);
  return bad;
}
