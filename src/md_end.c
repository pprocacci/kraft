#include "md.h"
#include "stralloc.h"

/* ends a reply: writes out anything held back and resets the terminal */
int
md_end(struct md *m, stralloc *o)
{
  static const struct md zero = MD_ZERO;

  if (m->star && !stralloc_append(o, '*')) return 0;
  while (m->hashes) {
    --m->hashes;
    if (!stralloc_append(o, '#')) return 0;
  }
  while (m->ticks) {
    --m->ticks;
    if (!stralloc_append(o, '`')) return 0;
  }
  *m = zero;
  return stralloc_cats(o, "\033[0m");
}
