#include "agent.h"

void
flush(void)
{
  if (buffer_flush(buffer_1) == -1)
    strerr_sys(111, FATAL, "unable to write output", 0);
}
