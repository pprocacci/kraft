#include "agent.h"

/* /mcp: the MCP servers, whether they are running, and their tools */
void
mcplist(void)
{
  unsigned long k, i, c;

  if (!nmcps) {
    outs("(no MCP servers; add \"mcp NAME COMMAND\" to ~/.kraft/config)\n");
    return;
  }
  for (k = 0; k < nmcps; ++k) {
    for (i = c = 0; i < nmcptools; ++i) c += mcptools[i].mcp == (int)k + 1;
    styled(BOLD, mcps[k].name);
    outs(mcps[k].pid ? "  running, " : "  not running, ");
    if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, c) ||
        !stralloc_cats(&tmp, c == 1 ? " tool" : " tools") || !stralloc_0(&tmp))
      nomem();
    outs(tmp.s);
    outs("\n");
    sgr(DIM);
    outs("  $ ");
    outs(mcps[k].cmd);
    outs("\n");
    sgr(0);
    for (i = 0; i < nmcptools; ++i)
      if (mcptools[i].mcp == (int)k + 1) {
        outs("  ");
        outs(mcptools[i].name);
        outs("\n");
      }
  }
}
