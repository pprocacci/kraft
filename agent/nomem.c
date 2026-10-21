#include "agent.h"

void
nomem(void)
{
  strerr_die(111, FATAL, "out of memory", 0);
}
