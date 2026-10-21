#include <limits.h>
#include "scan.h"

/* parses decimal digits; returns bytes used, 0 on none or overflow */
size_t
scan_ulong(const char *s, unsigned long *u)
{
  size_t i = 0;
  unsigned long r = 0, d;

  while (s[i] >= '0' && s[i] <= '9') {
    d = (unsigned long)(s[i] - '0');
    if (r > (ULONG_MAX - d) / 10) return 0;
    r = r * 10 + d;
    ++i;
  }
  if (i) *u = r;
  return i;
}
