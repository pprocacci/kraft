#include "agent.h"

/* forgets server k's login, here and on disk */
void
mcplogout(unsigned long k)
{
  static stralloc fn;

  mcpcredfn(k, &fn);
  mcps[k].token.len = 0;
  if (unlink(fn.s) == -1 && errno != ENOENT) {
    kwarnsys("unable to remove ", fn.s);
    return;
  }
  outs("(logged out of ");
  outs(mcps[k].name);
  outs("; the server may still hold the tokens until they run out)\n");
}
