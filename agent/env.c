#include "agent.h"

const char *
env(const char *name, const char *dflt)
{
  const char *s = getenv(name);

  return (s && *s) ? s : dflt;
}
