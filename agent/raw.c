#include "agent.h"

/* writes without counting as text: for terminal escapes */
void
raw(const char *s, size_t n)
{
  if (n && buffer_put(buffer_1, s, n) == -1)
    strerr_sys(111, FATAL, "unable to write output", 0);
}
