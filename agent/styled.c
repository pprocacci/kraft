#include "agent.h"

/* the same, with s written in between */
void
styled(const char *code, const char *s)
{
  sgr(code);
  outs(s);
  sgr(0);
}
