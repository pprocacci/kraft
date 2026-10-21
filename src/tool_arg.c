#include "json.h"
#include "stralloc.h"
#include "tool.h"

/*
 * copies the string argument key into out, NUL-terminated (the NUL
 * is not counted).  returns 1 if present, 0 if missing or not a string.
 */
int
tool_arg(const char *in, size_t n, const char *key, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(in, n, key, &v, &vn) || *v != '"') return 0;
  if (!stralloc_copys(out, "")) return 0;
  if (!json_unstr(out, v, vn)) return 0;
  if (!stralloc_0(out)) return 0;
  --out->len;
  return 1;
}
