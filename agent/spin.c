#include "agent.h"

/*
 * waits for fd to have something to read, showing a spinner and the
 * seconds since the request went out if that takes more than half a
 * second.  the line is cleared again before returning.
 */
void
spin(int fd)
{
  static const char *const frames[] = {
    "\342\240\213", "\342\240\231", "\342\240\271", "\342\240\270",
    "\342\240\274", "\342\240\264", "\342\240\246", "\342\240\247",
    "\342\240\207", "\342\240\217"
  };
  struct pollfd p;
  struct timespec now;
  unsigned long waited = 0, k = 0;
  int shown = 0;

  p.fd = fd;
  p.events = POLLIN;
  while (poll(&p, 1, 100) == 0) {
    if ((waited += 100) < 500) continue;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!stralloc_copys(&tmp, " thinking... ") ||
        !stralloc_catulong(&tmp, (unsigned long)(now.tv_sec - asked.tv_sec)) ||
        !stralloc_cats(&tmp, "s") || !stralloc_0(&tmp))
      nomem();
    raw("\r", 1);
    sgr(CYAN);
    raw(frames[k], str_len(frames[k]));
    sgr(DIM);
    raw(tmp.s, tmp.len - 1);
    sgr(0);
    flush();
    k = (k + 1) % (sizeof frames / sizeof frames[0]);
    shown = 1;
  }
  if (shown) {
    raw("\r\033[K", 4);
    flush();
  }
}
