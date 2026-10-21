#include <errno.h>
#include <string.h>
#include "proc.h"
#include "stralloc.h"
#include "tool.h"

#define MAXOUT 20000

/* grep: pattern, optional path and glob; runs the system grep(1) */
int
tool_grep(const char *in, size_t n, stralloc *out)
{
  stralloc pat = STRALLOC_ZERO, dir = STRALLOC_ZERO, glob = STRALLOC_ZERO;
  stralloc g = STRALLOC_ZERO;
  char *argv[12];
  int i = 0, status, ok = 0;

  if (!tool_arg(in, n, "pattern", &pat)) {
    stralloc_copys(out, "pattern is required");
    return 0;
  }
  if (!tool_arg(in, n, "path", &dir) || !dir.len)
    if (!stralloc_copys(&dir, ".") || !stralloc_0(&dir)) goto out;
  argv[i++] = "/usr/bin/grep";
  argv[i++] = "-rnIE";
  argv[i++] = "--exclude-dir=.git";
  if (tool_arg(in, n, "glob", &g) && g.len) {
    if (!stralloc_copys(&glob, "--include=") || !stralloc_cats(&glob, g.s) ||
        !stralloc_0(&glob))
      goto out;
    argv[i++] = glob.s;
  }
  argv[i++] = "-e";
  argv[i++] = pat.s;
  argv[i++] = "--";
  argv[i++] = dir.s;
  argv[i] = 0;

  stralloc_copys(out, "");
  if (!proc_run(argv, 60, MAXOUT, out, &status)) {
    stralloc_copys(out, "unable to run grep: ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  if (status == 1) {
    stralloc_copys(out, "no matches");
    ok = 1;
  } else if (status == -1) {
    stralloc_cats(out, "\n[grep took too long and was stopped]");
  } else if (status) {
    if (!out->len) stralloc_copys(out, "grep failed");
  } else
    ok = 1;
out:
  stralloc_free(&pat);
  stralloc_free(&dir);
  stralloc_free(&glob);
  stralloc_free(&g);
  return ok;
}
