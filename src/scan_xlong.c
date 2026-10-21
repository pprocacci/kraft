#include <limits.h>
#include "scan.h"

static int
hex(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* parses hex digits; returns bytes used, 0 on none or overflow */
size_t
scan_xlong(const char *s, unsigned long *u)
{
  size_t i = 0;
  unsigned long r = 0;
  int d;

  while ((d = hex((unsigned char)s[i])) >= 0) {
    if (r > (ULONG_MAX >> 4)) return 0;
    r = (r << 4) | (unsigned long)d;
    ++i;
  }
  if (i) *u = r;
  return i;
}
