#include "agent.h"

   /* the task under way, from 1 */

/* the undo directory goes with the session file; carries on its count */
void
setundo(void)
{
  stralloc p = STRALLOC_ZERO, d = STRALLOC_ZERO;
  unsigned long t;
  int ex;

  undoseq = taskno = 0;
  if (!stralloc_copys(&undodir, "")) nomem();
  if (sessfn.len < 6) return;
  if (!stralloc_copyb(&undodir, sessfn.s, str_len(sessfn.s) - 5) ||
      !stralloc_cats(&undodir, ".undo") || !stralloc_0(&undodir))
    nomem();
  --undodir.len;  /* NUL-terminated, but paths are built on the end of it */
  while (undo_load(undodir.s, undoseq + 1, &t, &p, &d, &ex)) {
    ++undoseq;
    if (t > taskno) taskno = t;
  }
  stralloc_free(&p);
  stralloc_free(&d);
}
