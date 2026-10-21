#include "agent.h"

#define STARTSECS 30
#define MAXDESC 4096

/* s, with what a tool name may not contain made _, onto out */
static void
safe(stralloc *out, const char *s, size_t n)
{
  char c;

  for (; n; ++s, --n) {
    c = *s;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-'))
      c = '_';
    if (!stralloc_append(out, c)) nomem();
  }
}

/* adds tool t (its JSON from tools/list) of server number k */
static void
addtool(unsigned long k, const char *t, size_t tn)
{
  static stralloc nm, full, d;
  struct tool *x;
  const char *v, *p;
  size_t vn, pn, k2;
  unsigned long i;

  if (!json_get(t, tn, "name", &v, &vn) || !stralloc_copys(&nm, "") ||
      !json_unstr(&nm, v, vn) || !nm.len)
    return;
  if (!stralloc_0(&nm)) nomem();
  if (!stralloc_copys(&full, "mcp__")) nomem();
  safe(&full, mcps[k].name, str_len(mcps[k].name));
  if (!stralloc_cats(&full, "__")) nomem();
  safe(&full, nm.s, nm.len - 1);
  if (full.len > 64) full.len = 64;
  if (!stralloc_0(&full)) nomem();
  for (i = 0; i < nmcptools && str_diff(mcptools[i].name, full.s); ++i) ;
  if (i < nmcptools) {
    kwarn("MCP: two tools would both be called ", full.s, "; skipping one");
    return;
  }
  if (nmcptools == MAXMCPTOOLS) {
    kwarn("MCP: too many tools; skipping ", full.s, 0);
    return;
  }
  x = &mcptools[nmcptools++];
  x->name = sdup(full.s);
  x->mname = sdup(nm.s);
  if (!stralloc_copys(&d, "")) nomem();
  if (json_get(t, tn, "description", &v, &vn) && !json_unstr(&d, v, vn))
    d.len = 0;
  if (d.len > MAXDESC) d.len = MAXDESC;
  if (!d.len && !stralloc_copys(&d, "(no description)")) nomem();
  if (!stralloc_0(&d)) nomem();
  x->desc = sdup(d.s);
  if (json_get(t, tn, "inputSchema", &v, &vn) && json_object(v, vn)) {
    /* some servers (LM Studio, for one) insist on properties */
    if (json_get(v, vn, "properties", &p, &pn)) {
      if (!stralloc_copyb(&d, v, vn)) nomem();
    } else {
      i = byte_chr(v, vn, '{') + 1;
      for (k2 = i; k2 < vn && (v[k2] == ' ' || v[k2] == '\t' ||
          v[k2] == '\n' || v[k2] == '\r'); ++k2) ;
      if (!stralloc_copyb(&d, v, i) ||
          !stralloc_cats(&d, v[k2] == '}' ? "\"properties\":{}" :
          "\"properties\":{},") || !stralloc_catb(&d, v + i, vn - i))
        nomem();
    }
    if (!stralloc_0(&d)) nomem();
    x->schema = sdup(d.s);
  } else
    x->schema = "{\"type\":\"object\",\"properties\":{}}";
  x->ask = 1;  /* there is no knowing what it does */
  x->mcp = (int)k + 1;
}

/* starts server k and learns its tools; 1 if all went well */
static int
start(unsigned long k, stralloc *why)
{
  static stralloc log, res, params;
  struct mcp *m = &mcps[k];
  const char *v, *home = env("HOME", 0);
  size_t vn;
  unsigned long i, pages;

  if (!stralloc_copys(&log, "")) nomem();
  if (home && sessdir(&tmp)) {  /* makes ~/.kraft if need be */
    if (!stralloc_copys(&log, home) || !stralloc_cats(&log, "/.kraft/mcp-"))
      nomem();
    safe(&log, m->name, str_len(m->name));
    if (!stralloc_cats(&log, ".log")) nomem();
  }
  if (!stralloc_0(&log)) nomem();
  if (!mcp_start(m, log.len > 1 ? log.s : 0, why)) return 0;
  if (!mcp_rpc(m, "initialize", "{\"protocolVersion\":\"2025-06-18\","
      "\"capabilities\":{},\"clientInfo\":{\"name\":\"kraft\",\"version\":\""
      KRAFT_VERSION "\"}}", why, STARTSECS, &intr))
    return 0;
  mcp_send(m, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}",
      str_len("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}"));
  if (!stralloc_copys(&params, "{}") || !stralloc_0(&params)) nomem();
  for (pages = 0; pages < 20; ++pages) {
    if (!mcp_rpc(m, "tools/list", params.s, &res, STARTSECS, &intr)) {
      if (!stralloc_copy(why, &res)) nomem();
      return 0;
    }
    for (i = 0; json_get(res.s, res.len, "tools", &v, &vn); ++i) {
      if (!stralloc_copys(&tmp, "tools.") || !stralloc_catulong(&tmp, i) ||
          !stralloc_0(&tmp))
        nomem();
      if (!json_get(res.s, res.len, tmp.s, &v, &vn)) break;
      addtool(k, v, vn);
    }
    if (!json_get(res.s, res.len, "nextCursor", &v, &vn) || *v != '"') break;
    if (!stralloc_copys(&params, "{\"cursor\":") ||
        !stralloc_catb(&params, v, vn) || !stralloc_cats(&params, "}") ||
        !stralloc_0(&params))
      nomem();
  }
  return 1;
}

/*
 * starts the MCP servers in the config, before settools() puts their
 * tools in the list.  one that fails is left out, with a warning.
 */
void
mcpstart(void)
{
  stralloc why = STRALLOC_ZERO;
  unsigned long k, before;

  for (k = 0; k < nmcps && !intr; ++k) {
    before = nmcptools;
    if (!stralloc_copys(&why, "")) nomem();
    if (!start(k, &why)) {
      nmcptools = before;
      mcp_stop(&mcps[k]);
      if (!stralloc_0(&why)) nomem();
      kwarn("MCP server ", mcps[k].name, intr ? ": interrupted" : 0);
      if (!intr) {
        sgr(DIM);
        outs("  (");
        outs(why.s);
        outs(mcps[k].http ? ")\n" : "; its stderr is in ~/.kraft/mcp-*.log)\n");
        sgr(0);
      }
    }
  }
  stralloc_free(&why);
}
