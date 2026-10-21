#include "json.h"

#define MAXDEPTH 128

static size_t
ws(const char *s, size_t n, size_t i)
{
  while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
      s[i] == '\r'))
    ++i;
  return i;
}

static size_t
lit(const char *s, size_t n, size_t i, const char *w)
{
  for (; *w; ++w, ++i)
    if (i >= n || s[i] != *w) return 0;
  return i;
}

static size_t
skip(const char *s, size_t n, size_t i, int depth)
{
  char close;

  i = ws(s, n, i);
  if (i >= n || depth > MAXDEPTH) return 0;
  switch (s[i]) {
  case '"':
    for (++i; i < n; ++i) {
      if (s[i] == '\\') { ++i; continue; }
      if (s[i] == '"') return i + 1;
    }
    return 0;
  case '{':
  case '[':
    close = s[i] == '{' ? '}' : ']';
    i = ws(s, n, i + 1);
    if (i < n && s[i] == close) return i + 1;
    for (;;) {
      if (close == '}') {
        i = ws(s, n, i);
        if (i >= n || s[i] != '"') return 0;
        if (!(i = skip(s, n, i, depth + 1))) return 0;
        i = ws(s, n, i);
        if (i >= n || s[i] != ':') return 0;
        ++i;
      }
      if (!(i = skip(s, n, i, depth + 1))) return 0;
      i = ws(s, n, i);
      if (i >= n) return 0;
      if (s[i] == close) return i + 1;
      if (s[i] != ',') return 0;
      ++i;
    }
  case 't': return lit(s, n, i, "true");
  case 'f': return lit(s, n, i, "false");
  case 'n': return lit(s, n, i, "null");
  default:
    if (s[i] != '-' && (s[i] < '0' || s[i] > '9')) return 0;
    for (++i; i < n; ++i)
      if (!((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' ||
          s[i] == 'E' || s[i] == '+' || s[i] == '-'))
        break;
    return i;
  }
}

/*
 * skips the JSON value at or after s[i] (leading whitespace allowed).
 * returns the index just past it, or 0 if it is malformed.
 */
size_t
json_skip(const char *s, size_t n, size_t i)
{
  return skip(s, n, i, 0);
}
