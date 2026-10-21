#include "str.h"
#include "buffer.h"

int
buffer_puts(buffer *b, const char *s)
{
  return buffer_put(b, s, str_len(s));
}
