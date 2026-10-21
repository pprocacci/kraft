#include "agent.h"

#define MAXOUT (256 * 1024)

/*
 * reads resource uri from the MCP server named server (n bytes) into
 * out, as text; 1, or 0 with the reason
 */
int
mcpread(const char *server, size_t n, const char *uri, stralloc *out)
{
  static stralloc params, res;
  const char *v;
  size_t vn;
  unsigned long k, i;

  for (k = 0; k < nmcps; ++k)
    if (str_len(mcps[k].name) == n && !byte_diff(mcps[k].name, n, server))
      break;
  if (k == nmcps) {
    if (!stralloc_copys(out, "there is no MCP server called ") ||
        !stralloc_catb(out, server, n))
      nomem();
    return 0;
  }
  if (!stralloc_copys(&params, "{\"uri\":") ||
      !json_str(&params, uri, str_len(uri)) || !stralloc_cats(&params, "}") ||
      !stralloc_0(&params))
    nomem();
  if (!mcp_rpc(&mcps[k], "resources/read", params.s, &res, 120, &intr)) {
    if (!stralloc_copys(out, "MCP server ") ||
        !stralloc_cats(out, mcps[k].name) || !stralloc_cats(out, ": ") ||
        !stralloc_cat(out, &res))
      nomem();
    return 0;
  }
  if (!stralloc_copys(out, "")) nomem();
  for (i = 0; out->len < MAXOUT; ++i) {
    if (!stralloc_copys(&tmp, "contents.") || !stralloc_catulong(&tmp, i) ||
        !stralloc_0(&tmp))
      nomem();
    if (!json_get(res.s, res.len, tmp.s, &v, &vn)) break;
    if (out->len && !stralloc_append(out, '\n')) nomem();
    if (json_get(v, vn, "text", &v, &vn)) {
      if (!json_unstr(out, v, vn)) nomem();
      continue;
    }
    if (!json_get(res.s, res.len, tmp.s, &v, &vn)) break;
    if (!stralloc_cats(out, "[binary")) nomem();
    if (json_get(v, vn, "mimeType", &v, &vn) &&
        (!stralloc_cats(out, " ") || !json_unstr(out, v, vn)))
      nomem();
    if (!stralloc_cats(out, ", not shown]")) nomem();
  }
  if (out->len > MAXOUT) {
    out->len = MAXOUT;
    if (!stralloc_cats(out, "\n[the rest was cut off]")) nomem();
  }
  if (!out->len && !stralloc_copys(out, "(empty)")) nomem();
  return 1;
}
