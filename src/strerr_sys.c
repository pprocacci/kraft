#include <errno.h>
#include <string.h>
#include "buffer.h"
#include "strerr.h"

/* like strerr_die, with ": " and the errno message appended */
void
strerr_sys(int e, const char *a, const char *b, const char *c)
{
  const char *m;

  m = strerror(errno);
  if (a) buffer_puts(buffer_2, a);
  if (b) buffer_puts(buffer_2, b);
  if (c) buffer_puts(buffer_2, c);
  strerr_die(e, ": ", m, 0);
}
