#include "job.h"
#include "stralloc.h"
#include "tool.h"

/* bash_kill: id.  stops a background job and forgets it */
int
tool_jobkill(const char *in, size_t n, stralloc *out)
{
  unsigned long id;

  if (!tool_argnum(in, n, "id", &id) || id > JOB_MAX || !job_kill((int)id)) {
    stralloc_copys(out, "there is no such background job");
    return 0;
  }
  stralloc_copys(out, "stopped background job ");
  stralloc_catulong(out, id);
  return 1;
}
