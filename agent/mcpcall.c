#include "agent.h"

#define CALLSECS 600
#define MAXOUT (256 * 1024)

/* one content item of a tool's result, as text, onto out */
static void
item(const char *c, size_t n, stralloc *out)
{
  const char *v;
  size_t vn;

  if (out->len && !stralloc_append(out, '\n')) nomem();
  if (json_get(c, n, "type", &v, &vn) && json_is(v, vn, "text") &&
      json_get(c, n, "text", &v, &vn)) {
    if (!json_unstr(out, v, vn)) nomem();
  } else if (json_get(c, n, "resource.text", &v, &vn)) {
    if (!json_unstr(out, v, vn)) nomem();
  } else {
    /* an image, audio, a link or a binary resource: just say so */
    if (!stralloc_cats(out, "[")) nomem();
    if (json_get(c, n, "type", &v, &vn) && !json_unstr(out, v, vn)) nomem();
    if ((json_get(c, n, "mimeType", &v, &vn) ||
        json_get(c, n, "resource.mimeType", &v, &vn)) &&
        (!stralloc_cats(out, " ") || !json_unstr(out, v, vn)))
      nomem();
    if ((json_get(c, n, "uri", &v, &vn) || json_get(c, n, "resource.uri",
        &v, &vn)) && (!stralloc_cats(out, " ") || !json_unstr(out, v, vn)))
      nomem();
    if (!stralloc_cats(out, " not shown]")) nomem();
  }
}

/* runs MCP tool t with input in; 1 with its output, 0 with the reason */
int
mcpcall(const struct tool *t, const char *in, size_t n, stralloc *out)
{
  static stralloc params, res;
  struct mcp *m = &mcps[t->mcp - 1];
  const char *v;
  size_t vn;
  unsigned long i;
  int r;

  if (!stralloc_copys(&params, "{\"name\":") ||
      !json_str(&params, t->mname, str_len(t->mname)) ||
      !stralloc_cats(&params, ",\"arguments\":") ||
      !stralloc_catb(&params, in, n) || !stralloc_cats(&params, "}") ||
      !stralloc_0(&params))
    nomem();
  plock(1);  /* one at a time on its pipes */
  r = mcp_rpc(m, "tools/call", params.s, &res, CALLSECS, &intr);
  if (!r && m->http && m->status == 401 && mcpauth((unsigned long)(t->mcp - 1), 1))
    r = mcp_rpc(m, "tools/call", params.s, &res, CALLSECS, &intr);  /* renewed */
  plock(0);
  if (!r) {
    if (!stralloc_copys(out, "MCP server ") || !stralloc_cats(out, m->name) ||
        !stralloc_cats(out, ": ") || !stralloc_cat(out, &res))
      nomem();
    return 0;
  }
  if (!stralloc_copys(out, "")) nomem();
  for (i = 0; out->len < MAXOUT; ++i) {
    if (!stralloc_copys(&tmp, "content.") || !stralloc_catulong(&tmp, i) ||
        !stralloc_0(&tmp))
      nomem();
    if (!json_get(res.s, res.len, tmp.s, &v, &vn)) break;
    item(v, vn, out);
  }
  if (!out->len && json_get(res.s, res.len, "structuredContent", &v, &vn))
    if (!stralloc_copyb(out, v, vn)) nomem();
  if (out->len > MAXOUT) {
    out->len = MAXOUT;
    if (!stralloc_cats(out, "\n[the rest was cut off]")) nomem();
  }
  if (!out->len && !stralloc_copys(out, "(no output)")) nomem();
  return !(json_get(res.s, res.len, "isError", &v, &vn) &&
      vn == 4 && !byte_diff(v, 4, "true"));
}
