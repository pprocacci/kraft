#include "agent.h"

  /* the working directory is in a git work tree */

/* runs cmd with /bin/sh, its output into out; 1 if it exited 0 */
int
sh(const char *cmd, stralloc *out)
{
  char *argv[4];
  int status;

  argv[0] = "/bin/sh";
  argv[1] = "-c";
  argv[2] = (char *)cmd;
  argv[3] = 0;
  if (!stralloc_copys(out, "")) nomem();
  if (!proc_run(argv, 30, 1024 * 1024, out, &status)) return 0;
  while (out->len && out->s[out->len - 1] == '\n') --out->len;
  if (!stralloc_0(out)) nomem();
  --out->len;
  return status == 0;
}
