#include "json.h"
#include "scan.h"
#include "tool.h"

/* sets *u from the whole-number argument key; 1 if present and valid */
int
tool_argnum(const char *in, size_t n, const char *key, unsigned long *u)
{
  const char *v;
  size_t vn;
  unsigned long r;

  if (!json_get(in, n, key, &v, &vn)) return 0;
  if (*v == '"') { ++v; vn -= 2; }  /* some models quote numbers */
  if (!vn || scan_ulongb(v, vn, &r) != vn) return 0;
  *u = r;
  return 1;
}
