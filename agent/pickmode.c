#include "agent.h"

int
pickmode(const char *s)
{
  int i;

  for (i = 0; modes[i]; ++i)
    if (!str_diff(s, modes[i])) { mode = i; return 1; }
  return 0;
}
