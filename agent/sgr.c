#include "agent.h"

void
sgr(const char *code)
{
  if (!color) return;
  raw("\033[", 2);
  if (code) raw(code, str_len(code));
  raw("m", 1);
}
