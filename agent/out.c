#include "agent.h"

void
out(const char *s, size_t n)
{
  if (!n) return;
  last = s[n - 1];
  raw(s, n);
}
