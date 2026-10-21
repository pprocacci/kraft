#include "byte.h"
#include "json.h"
#include "tool.h"

/* 1 if the argument key is true (or "true"), 0 otherwise */
int
tool_argflag(const char *in, size_t n, const char *key)
{
  const char *v;
  size_t vn;

  if (!json_get(in, n, key, &v, &vn)) return 0;
  return (vn == 4 && !byte_diff(v, 4, "true")) || json_is(v, vn, "true");
}
