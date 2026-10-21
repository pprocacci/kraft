#include "fmt.h"

/* writes u in decimal to s (if s is not null); returns the length */
size_t
fmt_ulong(char *s, unsigned long u)
{
  size_t len = 1;
  unsigned long q = u;

  while (q > 9) { ++len; q /= 10; }
  if (s) {
    s += len;
    do { *--s = '0' + (char)(u % 10); u /= 10; } while (u);
  }
  return len;
}
