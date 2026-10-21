#include "agent.h"

void
kwarnsys(const char *a, const char *b)
{
  const char *e = strerror(errno);

  if (!stralloc_copys(&tmp, b) || !stralloc_cats(&tmp, ": ") ||
      !stralloc_cats(&tmp, e) || !stralloc_0(&tmp))
    nomem();
  kwarn(a, tmp.s, 0);
}
