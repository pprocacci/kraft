#include <limits.h>
#include "scan.h"

/*
 * as scan_ulong, but reading no more than the n bytes of s, which need
 * not be NUL-terminated: for numbers inside a larger buffer, such as
 * a JSON value.  returns bytes used, 0 on none or overflow.
 */
size_t
scan_ulongb(const char *s, size_t n, unsigned long *u)
{
  size_t i = 0;
  unsigned long r = 0, d;

  while (i < n && s[i] >= '0' && s[i] <= '9') {
    d = (unsigned long)(s[i] - '0');
    if (r > (ULONG_MAX - d) / 10) return 0;
    r = r * 10 + d;
    ++i;
  }
  if (i) *u = r;
  return i;
}
