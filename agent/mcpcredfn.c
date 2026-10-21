#include "agent.h"

/* ~/.kraft/mcp-auth/NAME.json, server k's login, NUL-terminated */
void
mcpcredfn(unsigned long k, stralloc *fn)
{
  const char *s;

  if (!stralloc_copys(fn, env("HOME", "")) ||
      !stralloc_cats(fn, "/.kraft/mcp-auth/"))
    nomem();
  for (s = mcps[k].name; *s; ++s)
    if (!stralloc_append(fn, (*s >= 'a' && *s <= 'z') || (*s >= 'A' &&
        *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-' ? *s : '_'))
      nomem();
  if (!stralloc_cats(fn, ".json") || !stralloc_0(fn)) nomem();
}
