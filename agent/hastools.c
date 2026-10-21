#include "agent.h"

int
hastools(const struct msg *m)
{
  size_t i;

  for (i = 0; i < m->n; ++i)
    if (m->b[i].type == BLK_TOOL) return 1;
  return 0;
}
