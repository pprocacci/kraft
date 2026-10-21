#include "agent.h"

/*
 * appends the session's cost so far to s, as "$1.2345", with "+" if
 * some requests could not be priced, or nothing if none could.
 */
void
catcost(stralloc *s)
{
  unsigned long long c = cost / 10000;  /* in hundredths of a cent */
  unsigned long f;
  char d[5];
  int i;

  if (!cost && costgap) return;
  f = (unsigned long)(c % 10000);
  for (i = 3; i >= 0; --i) { d[i] = '0' + (char)(f % 10); f /= 10; }
  d[4] = 0;
  if (!stralloc_cats(s, "$") ||
      !stralloc_catulong(s, (unsigned long)(c / 10000)) ||
      !stralloc_cats(s, ".") || !stralloc_cats(s, d) ||
      (costgap && !stralloc_cats(s, "+")))
    nomem();
}
