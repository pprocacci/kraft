#include "oauth.h"
#include "str.h"
#include "stralloc.h"

static int
lower(char c)
{
  return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/*
 * finds auth-param name in a WWW-Authenticate value h (n bytes), such
 * as 'Bearer resource_metadata="https://...", scope="a b"' (RFC 9110:
 * a token or a quoted string, names without regard to case), and
 * puts its value, NUL-terminated, in out.  returns 1 if it was there.
 */
int
www_param(const char *h, size_t n, const char *name, stralloc *out)
{
  size_t i = 0, s, k = str_len(name), j;
  int q;

  while (i < n) {
    /* a name: up to = , or space */
    while (i < n && (h[i] == ' ' || h[i] == ',' || h[i] == '\t')) ++i;
    s = i;
    while (i < n && h[i] != '=' && h[i] != ',' && h[i] != ' ' && h[i] != '\t')
      ++i;
    j = i;
    while (i < n && (h[i] == ' ' || h[i] == '\t')) ++i;
    if (i >= n || h[i] != '=') continue;  /* the scheme, or a token68 */
    for (++i; i < n && (h[i] == ' ' || h[i] == '\t'); ++i) ;
    q = i < n && h[i] == '"';
    if (!stralloc_copys(out, "")) return 0;
    for (i += q; i < n; ++i) {
      if (q && h[i] == '"') { ++i; break; }
      if (!q && (h[i] == ',' || h[i] == ' ' || h[i] == '\t')) break;
      if (q && h[i] == '\\' && i + 1 < n) ++i;
      if (!stralloc_append(out, h[i])) return 0;
    }
    if (j - s == k) {
      size_t m;

      for (m = 0; m < k && lower(h[s + m]) == lower(name[m]); ++m) ;
      if (m == k) {
        if (!stralloc_0(out)) return 0;
        --out->len;
        return 1;
      }
    }
  }
  out->len = 0;  /* not the last one's value */
  return 0;
}
