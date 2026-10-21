#include "oauth.h"
#include "stralloc.h"

/*
 * appends s (n bytes) to out percent-encoded, as for a query or a
 * form: everything but letters, digits and -._~ becomes %XX
 */
int
url_enc(stralloc *out, const char *s, size_t n)
{
  static const char hex[] = "0123456789ABCDEF";
  unsigned char c;

  for (; n; --n, ++s) {
    c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
      if (!stralloc_append(out, (char)c)) return 0;
    } else if (!stralloc_append(out, '%') ||
        !stralloc_append(out, hex[c >> 4]) ||
        !stralloc_append(out, hex[c & 15]))
      return 0;
  }
  return 1;
}
