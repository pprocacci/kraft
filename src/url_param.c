#include "byte.h"
#include "oauth.h"
#include "str.h"
#include "stralloc.h"

static int
hexval(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/*
 * finds parameter name in q (n bytes), a query string or form body
 * such as "a=1&b=x%20y" (a leading ? or anything up to it is
 * skipped), and puts its value, decoded and NUL-terminated, in out.
 * returns 1 if it was there.
 */
int
url_param(const char *q, size_t n, const char *name, stralloc *out)
{
  size_t i, e, k = str_len(name), v;
  int h, l;

  i = byte_chr(q, n, '?');
  i = i < n ? i + 1 : 0;
  for (; i < n; i = e + 1) {
    e = i + byte_chr(q + i, n - i, '&');
    v = i + byte_chr(q + i, e - i, '=');
    if (v - i != k || byte_diff(q + i, k, name)) continue;
    if (!stralloc_copys(out, "")) return 0;
    for (i = v < e ? v + 1 : e; i < e; ++i) {
      if (q[i] == '+') {
        if (!stralloc_append(out, ' ')) return 0;
      } else if (q[i] == '%' && e - i >= 3 && (h = hexval(q[i + 1])) >= 0 &&
          (l = hexval(q[i + 2])) >= 0) {
        if (!stralloc_append(out, (char)(h * 16 + l))) return 0;
        i += 2;
      } else if (q[i] == '#') {
        break;  /* a fragment, if the whole URL was pasted */
      } else if (!stralloc_append(out, q[i]))
        return 0;
    }
    if (!stralloc_0(out)) return 0;
    --out->len;
    return 1;
  }
  return 0;
}
