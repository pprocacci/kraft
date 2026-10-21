#include "agent.h"

/* ~/.kraft/sessions/, created if need be; 0 if there is no $HOME */
int
sessdir(stralloc *d)
{
  const char *home = env("HOME", 0);

  if (!home) return 0;
  if (!stralloc_copys(d, home) || !stralloc_cats(d, "/.kraft") ||
      !stralloc_0(d))
    nomem();
  mkdir(d->s, 0700);
  --d->len;
  if (!stralloc_cats(d, "/sessions") || !stralloc_0(d)) nomem();
  mkdir(d->s, 0700);
  --d->len;
  return 1;
}
