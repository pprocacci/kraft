#include "agent.h"

/* waits secs seconds, or until ^C; 0 if interrupted */
static int
pause_for(unsigned long secs)
{
  unsigned long ms;

  for (ms = 0; ms < secs * 1000 && !intr; ms += 100)
    poll(0, 0, 100);
  return !intr;
}

/*
 * sends conversation c, with system prompt sysp and tools, to model
 * mdl, and streams the reply into a new assistant message, printing
 * it as it arrives if show is set.  returns 1 on success.  on failure
 * the partial reply is dropped, so the conversation stays valid, and
 * 0 is returned.  if the user interrupts, whatever text arrived is
 * kept and -1 is returned.  a failure that may pass (no connection,
 * an overloaded or broken server, a stream cut short) is tried again
 * up to RETRIES times, waiting 2, 4, 8... seconds, or as long as the
 * server asks.
 */
int
exchange(struct conv *c, const char *sysp, const struct tool *tools,
    const char *mdl, unsigned long mx, int show)
{
  unsigned long secs;
  int n, r;

  for (n = 0;; ++n) {
    again = 0;
    waitsecs = 0;
    r = attempt(c, sysp, tools, mdl, mx, show);
    if (r || !again || n == RETRIES) return r;
    secs = waitsecs ? waitsecs : 2UL << n;
    if (secs > 120) secs = 120;
    if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, secs) ||
        !stralloc_cats(&tmp, "s (try ") ||
        !stralloc_catulong(&tmp, (unsigned long)n + 2) ||
        !stralloc_cats(&tmp, " of ") ||
        !stralloc_catulong(&tmp, RETRIES + 1) || !stralloc_cats(&tmp, ")") ||
        !stralloc_0(&tmp))
      nomem();
    kwarn("trying again in ", tmp.s, 0);
    if (!pause_for(secs)) {
      kwarn("interrupted", 0, 0);
      return -1;
    }
    status = 0;
  }
}
