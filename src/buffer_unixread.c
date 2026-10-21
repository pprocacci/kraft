#include <errno.h>
#include <unistd.h>
#include "buffer.h"

ssize_t
buffer_unixread(int fd, char *s, size_t n)
{
  ssize_t r;

  do r = read(fd, s, n); while (r == -1 && errno == EINTR);
  return r;
}
