#include "json.h"
#include "byte.h"
#include "scan.h"

static size_t
ws(const char *s, size_t n, size_t i)
{
  while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
      s[i] == '\r'))
    ++i;
  return i;
}

/* finds key k (klen bytes) in the object at s[i]; returns value index */
static size_t
member(const char *s, size_t n, size_t i, const char *k, size_t klen)
{
  size_t ks, ke;

  if (i >= n || s[i] != '{') return 0;
  i = ws(s, n, i + 1);
  if (i < n && s[i] == '}') return 0;
  for (;;) {
    ks = i;
    if (!(ke = json_skip(s, n, i))) return 0;
    i = ws(s, n, ke);
    if (i >= n || s[i] != ':') return 0;
    i = ws(s, n, i + 1);
    if (ke - ks - 2 == klen && !byte_diff(s + ks + 1, klen, k)) return i;
    if (!(i = json_skip(s, n, i))) return 0;
    i = ws(s, n, i);
    if (i >= n || s[i] != ',') return 0;
    i = ws(s, n, i + 1);
  }
}

/* finds element idx in the array at s[i]; returns its index */
static size_t
element(const char *s, size_t n, size_t i, unsigned long idx)
{
  if (i >= n || s[i] != '[') return 0;
  i = ws(s, n, i + 1);
  if (i < n && s[i] == ']') return 0;
  for (;;) {
    if (!idx--) return i;
    if (!(i = json_skip(s, n, i))) return 0;
    i = ws(s, n, i);
    if (i >= n || s[i] != ',') return 0;
    i = ws(s, n, i + 1);
  }
}

/*
 * finds the value at a dotted path such as "delta.text" or
 * "choices.0.delta.content" (numbers index arrays).  sets *v and
 * *vlen to its raw JSON text.  returns 1 if found, 0 otherwise.
 */
int
json_get(const char *s, size_t n, const char *path, const char **v,
    size_t *vlen)
{
  const char *seg;
  size_t i, len, e;
  unsigned long idx;

  i = ws(s, n, 0);
  while (*path) {
    seg = path;
    while (*path && *path != '.') ++path;
    len = (size_t)(path - seg);
    if (*path) ++path;
    if (i < n && s[i] == '[') {
      if (scan_ulong(seg, &idx) != len) return 0;
      i = element(s, n, i, idx);
    } else
      i = member(s, n, i, seg, len);
    if (!i) return 0;
  }
  if (!(e = json_skip(s, n, i))) return 0;
  *v = s + i;
  *vlen = e - i;
  return 1;
}
