#include "agent.h"

/*
 * sets the effort asked of the model: low, medium, high, xhigh or
 * max, or "default" to leave it to the server.  0 if s is none of
 * those.
 */
int
pickeffort(const char *s)
{
  static const char *const levels[] = {
    "low", "medium", "high", "xhigh", "max", 0
  };
  int i;

  if (!str_diff(s, "default")) { effort = 0; return 1; }
  for (i = 0; levels[i]; ++i)
    if (!str_diff(s, levels[i])) { effort = levels[i]; return 1; }
  return 0;
}
