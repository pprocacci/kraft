#include "json.h"
#include "stralloc.h"

static long
hex4(const char *s, size_t n)
{
  long r = 0;
  size_t i;
  char c;

  if (n < 4) return -1;
  for (i = 0; i < 4; ++i) {
    c = s[i];
    r <<= 4;
    if (c >= '0' && c <= '9') r |= c - '0';
    else if (c >= 'a' && c <= 'f') r |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') r |= c - 'A' + 10;
    else return -1;
  }
  return r;
}

static int
utf8(stralloc *out, unsigned long u)
{
  char b[4];
  size_t n;

  if (u < 0x80) { b[0] = (char)u; n = 1; }
  else if (u < 0x800) {
    b[0] = (char)(0xc0 | (u >> 6));
    b[1] = (char)(0x80 | (u & 0x3f));
    n = 2;
  } else if (u < 0x10000) {
    b[0] = (char)(0xe0 | (u >> 12));
    b[1] = (char)(0x80 | ((u >> 6) & 0x3f));
    b[2] = (char)(0x80 | (u & 0x3f));
    n = 3;
  } else {
    b[0] = (char)(0xf0 | (u >> 18));
    b[1] = (char)(0x80 | ((u >> 12) & 0x3f));
    b[2] = (char)(0x80 | ((u >> 6) & 0x3f));
    b[3] = (char)(0x80 | (u & 0x3f));
    n = 4;
  }
  return stralloc_catb(out, b, n);
}

/*
 * appends the decoded contents of the JSON string token v (quotes
 * included) to out.  returns 1 on success, 0 if v is not a valid
 * string or memory runs out.
 */
int
json_unstr(stralloc *out, const char *v, size_t n)
{
  size_t i;
  long u, l;
  char c;

  if (n < 2 || v[0] != '"' || v[n - 1] != '"') return 0;
  --n;
  for (i = 1; i < n; ++i) {
    c = v[i];
    if (c != '\\') {
      if (!stralloc_append(out, c)) return 0;
      continue;
    }
    if (++i >= n) return 0;
    switch (v[i]) {
    case '"': c = '"'; break;
    case '\\': c = '\\'; break;
    case '/': c = '/'; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    case 'n': c = '\n'; break;
    case 'r': c = '\r'; break;
    case 't': c = '\t'; break;
    case 'u':
      if ((u = hex4(v + i + 1, n - i - 1)) < 0) return 0;
      i += 4;
      if (u >= 0xd800 && u < 0xdc00) {
        /* high surrogate: expect a low one next */
        if (i + 2 < n && v[i + 1] == '\\' && v[i + 2] == 'u' &&
            (l = hex4(v + i + 3, n - i - 3)) >= 0xdc00 && l < 0xe000) {
          u = 0x10000 + ((u - 0xd800) << 10) + (l - 0xdc00);
          i += 6;
        } else u = 0xfffd;
      } else if (u >= 0xdc00 && u < 0xe000) u = 0xfffd;
      if (!utf8(out, (unsigned long)u)) return 0;
      continue;
    default:
      return 0;
    }
    if (!stralloc_append(out, c)) return 0;
  }
  return 1;
}
