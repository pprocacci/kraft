#include <stdlib.h>
#include "buffer.h"
#include "strerr.h"

/* prints a, b and c (any may be null) on stderr and exits with e */
void
strerr_die(int e, const char *a, const char *b, const char *c)
{
  buffer_flush(buffer_1);
  if (a) buffer_puts(buffer_2, a);
  if (b) buffer_puts(buffer_2, b);
  if (c) buffer_puts(buffer_2, c);
  buffer_puts(buffer_2, "\n");
  buffer_flush(buffer_2);
  _Exit(e);
}
