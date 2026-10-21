#include "agent.h"

/* the read_resource tool: an MCP server's resource, by server and URI */
int
readresource(const char *in, size_t n, stralloc *out)
{
  static stralloc server, uri;

  if (!tool_arg(in, n, "server", &server) || !tool_arg(in, n, "uri", &uri)) {
    if (!stralloc_copys(out, "a server and a uri are needed")) nomem();
    return 0;
  }
  return mcpread(server.s, server.len, uri.s, out);
}
