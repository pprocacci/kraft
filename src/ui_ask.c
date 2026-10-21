#include <fcntl.h>
#include <unistd.h>
#include "buffer.h"
#include "str.h"
#include "ui.h"

/*
 * asks question q on the terminal and returns 'y', 'n' or 'a' (for
 * "always"); any other answer counts as 'n'.  returns 0 if there is
 * no terminal to ask on.
 */
int
ui_ask(const char *q)
{
  char buf[64], c = 0;
  ssize_t r;
  size_t i;
  int fd;

  buffer_flush(buffer_1);
  fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
  if (fd == -1) return 0;
  buffer_unixwrite(fd, (char *)q, str_len(q));
  r = buffer_unixread(fd, buf, sizeof buf);
  close(fd);
  for (i = 0; r > 0 && i < (size_t)r; ++i)
    if (buf[i] != ' ' && buf[i] != '\t') { c = buf[i]; break; }
  if (c == 'Y') c = 'y';
  if (c == 'A') c = 'a';
  return (c == 'y' || c == 'a') ? c : 'n';
}
