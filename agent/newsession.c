#include "agent.h"

/* picks a new session file; nothing is written until there is a message */
void
newsession(void)
{
  char stamp[32];
  time_t now = time(0);

  if (!stralloc_copys(&sessfn, "")) nomem();
  if (!sessdir(&sessfn)) return;
  if (!strftime(stamp, sizeof stamp, "/%Y%m%d-%H%M%S-", localtime(&now)))
    stamp[0] = 0;
  if (!stralloc_cats(&sessfn, stamp) ||
      !stralloc_catulong(&sessfn, (unsigned long)getpid()) ||
      !stralloc_cats(&sessfn, ".json") || !stralloc_0(&sessfn))
    nomem();
  setundo();
}
