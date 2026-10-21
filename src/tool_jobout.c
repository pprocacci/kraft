#include "job.h"
#include "stralloc.h"
#include "tool.h"

#define MAXOUT 30000

/*
 * bash_output: id, optional wait (seconds, at most 120).  without an
 * id, lists the background jobs.
 */
int
tool_jobout(const char *in, size_t n, stralloc *out)
{
  unsigned long id, secs = 0;
  int i, any = 0;

  stralloc_copys(out, "");
  if (!tool_argnum(in, n, "id", &id)) {
    for (i = 0; i < JOB_MAX; ++i) {
      if (!job_tab[i].pid) continue;
      any = 1;
      stralloc_catulong(out, (unsigned long)i + 1);
      stralloc_cats(out, job_tab[i].done ? " (finished): " : " (running): ");
      stralloc_cats(out, job_tab[i].cmd.s);
      stralloc_append(out, '\n');
    }
    if (!any) stralloc_cats(out, "no background jobs");
    return 1;
  }
  tool_argnum(in, n, "wait", &secs);
  if (secs > 120) secs = 120;
  if (id > JOB_MAX || !job_output((int)id, secs, MAXOUT, out)) {
    stralloc_copys(out, "there is no background job ");
    stralloc_catulong(out, id);
    return 0;
  }
  return 1;
}
