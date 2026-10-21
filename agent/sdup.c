#include "agent.h"

char *
sdup(const char *s)
{
  size_t n = str_len(s) + 1;
  char *d = alloc(n);

  if (!d) nomem();
  byte_copy(d, n, s);
  return d;
}
