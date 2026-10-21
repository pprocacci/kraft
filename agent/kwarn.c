#include "agent.h"

void
kwarn(const char *a, const char *b, const char *c)
{
  nl();
  flush();
  buffer_puts(buffer_2, "kraft: ");
  if (a) buffer_puts(buffer_2, a);
  if (b) buffer_puts(buffer_2, b);
  if (c) buffer_puts(buffer_2, c);
  buffer_puts(buffer_2, "\n");
  buffer_flush(buffer_2);
}
