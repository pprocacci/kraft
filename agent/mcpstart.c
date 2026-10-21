#include "agent.h"

/* why server k did not start, from why */
static void
failed(unsigned long k, stralloc *why)
{
  if (!stralloc_0(why)) nomem();
  kwarn("MCP server ", mcps[k].name, intr ? ": interrupted" : 0);
  if (intr) return;
  sgr(DIM);
  outs("  (");
  outs(why->s);
  outs(mcps[k].http ? ")\n" : "; its stderr is in ~/.kraft/mcp-*.log)\n");
  sgr(0);
}

/*
 * starts the MCP servers in the config, before settools() puts their
 * tools in the list.  an HTTP server that wants a login gets the
 * saved one, renewed if need be, or, at a terminal, the offer of a
 * login now.  one that fails is left out, with a warning.
 */
void
mcpstart(void)
{
  stralloc why = STRALLOC_ZERO;
  unsigned long k;
  int c, ok;

  for (k = 0; k < nmcps && !intr; ++k) {
    if (!stralloc_copys(&why, "")) nomem();
    mcpauth(k, 0);
    ok = mcpup(k, &why);
    if (!ok && mcps[k].status == 401 && mcps[k].token.len && mcpauth(k, 1)) {
      if (!stralloc_copys(&why, "")) nomem();
      ok = mcpup(k, &why);
    }
    if (!ok && mcps[k].status == 401 && !script && isatty(0) && isatty(1)) {
      flush();
      outs("MCP server ");
      styled(BOLD, mcps[k].name);
      outs(" wants you to log in.\n");
      flush();
      c = ui_ask(color ? "  \033[1;33mLog in now?\033[0m [y]es, [n]o: " :
          "  Log in now? [y]es, [n]o: ");
      last = '\n';
      if (c == 'y' && mcplogin(k)) {
        if (!stralloc_copys(&why, "")) nomem();
        ok = mcpup(k, &why);
      } else if (c != 'y') {
        outs("  (/mcp login ");
        outs(mcps[k].name);
        outs(" when you want it)\n");
        continue;
      }
    }
    if (!ok) failed(k, &why);
  }
  stralloc_free(&why);
}
