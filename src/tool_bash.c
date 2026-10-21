#include <errno.h>
#include <string.h>
#include "job.h"
#include "proc.h"
#include "stralloc.h"
#include "tool.h"

#define MAXOUT 30000

/* bash: command, optional timeout in seconds */
int
tool_bash(const char *in, size_t n, stralloc *out)
{
  stralloc cmd = STRALLOC_ZERO;
  unsigned long secs = 120;
  char *argv[4];
  int status, ok = 0;

  if (!tool_arg(in, n, "command", &cmd)) {
    stralloc_copys(out, "command is required");
    return 0;
  }
  if (tool_argflag(in, n, "background")) {
    int id;

    stralloc_copys(out, "");
    if (!(id = job_start(cmd.s, out))) goto out;
    stralloc_copys(out, "started background job ");
    stralloc_catulong(out, (unsigned long)id);
    stralloc_cats(out, "; read its output with bash_output (id ");
    stralloc_catulong(out, (unsigned long)id);
    stralloc_cats(out, ") and stop it with bash_kill");
    ok = 1;
    goto out;
  }
  tool_argnum(in, n, "timeout", &secs);
  if (!secs) secs = 120;
  if (secs > 600) secs = 600;

  argv[0] = "/bin/sh";
  argv[1] = "-c";
  argv[2] = cmd.s;
  argv[3] = 0;
  stralloc_copys(out, "");
  if (!proc_run(argv, secs, MAXOUT, out, &status)) {
    stralloc_copys(out, "unable to run /bin/sh: ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  if (status == -1) {
    stralloc_cats(out, "\n[killed after ");
    stralloc_catulong(out, secs);
    stralloc_cats(out, " seconds]");
    goto out;
  }
  if (status) {
    stralloc_cats(out, out->len ? "\n[exit status " : "[exit status ");
    stralloc_catulong(out, (unsigned long)status);
    stralloc_cats(out, "]");
  } else if (!out->len)
    stralloc_cats(out, "(no output)");
  ok = 1;
out:
  stralloc_free(&cmd);
  return ok;
}
