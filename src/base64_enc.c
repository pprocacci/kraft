#include "media.h"
#include "stralloc.h"

/* appends s, base64-encoded with padding and no line breaks, to out */
int
base64_enc(stralloc *out, const char *s, size_t n)
{
  static const char t[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char *u = (const unsigned char *)s;
  unsigned long v;
  char q[4];
  size_t i;

  if (!stralloc_readyplus(out, (n + 2) / 3 * 4 + 1)) return 0;
  for (i = 0; i + 2 < n; i += 3) {
    v = (unsigned long)u[i] << 16 | (unsigned long)u[i + 1] << 8 | u[i + 2];
    q[0] = t[v >> 18];
    q[1] = t[(v >> 12) & 63];
    q[2] = t[(v >> 6) & 63];
    q[3] = t[v & 63];
    if (!stralloc_catb(out, q, 4)) return 0;
  }
  if (i < n) {
    v = (unsigned long)u[i] << 16;
    if (i + 1 < n) v |= (unsigned long)u[i + 1] << 8;
    q[0] = t[v >> 18];
    q[1] = t[(v >> 12) & 63];
    q[2] = i + 1 < n ? t[(v >> 6) & 63] : '=';
    q[3] = '=';
    if (!stralloc_catb(out, q, 4)) return 0;
  }
  return 1;
}
