#include <sys/types.h>
#include <sys/resource.h>
#include "proc.h"

/*
 * limits for what the model runs, generous enough for big builds:
 * CPU seconds per process, the largest file it may write, the user's
 * processes (bounding a fork bomb), and no core dumps.  a limit is
 * only ever lowered.
 */
static const struct { int res; rlim_t max; } limits[] = {
  { RLIMIT_CPU, 3600 },
  { RLIMIT_FSIZE, (rlim_t)4 << 30 },
  { RLIMIT_NPROC, 2048 },
  { RLIMIT_CORE, 0 },
};

/* applies the limits to this process; for a child, before exec */
void
proc_limit(void)
{
  struct rlimit rl;
  size_t i;

  for (i = 0; i < sizeof limits / sizeof limits[0]; ++i) {
    if (getrlimit(limits[i].res, &rl) == -1) continue;
    if (rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur > limits[i].max)
      rl.rlim_cur = limits[i].max;
    if (rl.rlim_max == RLIM_INFINITY || rl.rlim_max > limits[i].max)
      rl.rlim_max = limits[i].max;
    setrlimit(limits[i].res, &rl);
  }
}
