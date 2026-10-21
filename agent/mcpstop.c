#include "agent.h"

/* stops the MCP servers; they do not outlive kraft */
void
mcpstop(void)
{
  unsigned long k;

  for (k = 0; k < nmcps; ++k) mcp_stop(&mcps[k]);
}
