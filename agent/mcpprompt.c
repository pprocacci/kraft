#include "agent.h"

/* the prompt's text, from prompts/get's messages, onto out */
static void
texts(const char *r, size_t rn, stralloc *out)
{
  const char *v;
  size_t vn;
  unsigned long i;

  for (i = 0; ; ++i) {
    if (!stralloc_copys(&tmp, "messages.") || !stralloc_catulong(&tmp, i) ||
        !stralloc_cats(&tmp, ".content") || !stralloc_0(&tmp))
      nomem();
    if (!json_get(r, rn, tmp.s, &v, &vn)) break;
    if (!json_get(v, vn, "text", &v, &vn) &&
        !(json_get(r, rn, tmp.s, &v, &vn) &&
        json_get(v, vn, "resource.text", &v, &vn)))
      continue;  /* an image or the like */
    if (out->len && !stralloc_cats(out, "\n\n")) nomem();
    if (!json_unstr(out, v, vn)) nomem();
  }
}

/*
 * runs line, "/server:prompt arguments", if it names an MCP server's
 * prompt: the arguments, separated by spaces, fill the prompt's in
 * order, the last taking the rest of the line.  the prompt the server
 * makes is the task.  returns 1 if line was for one.
 */
int
mcpprompt(const char *line)
{
  static stralloc params, res, text;
  const struct mcpprompt *p = 0;
  const char *name = line + 1, *a;
  size_t n, c, e;
  unsigned long i;
  int k;

  for (n = 0; name[n] && name[n] != ' '; ++n) ;
  c = byte_chr(name, n, ':');
  if (c == n) return 0;
  for (i = 0; i < nmcpprompts && !p; ++i) {
    k = mcpprompts[i].srv - 1;
    if (str_len(mcps[k].name) == c && !byte_diff(mcps[k].name, c, name) &&
        str_len(mcpprompts[i].name) == n - c - 1 &&
        !byte_diff(mcpprompts[i].name, n - c - 1, name + c + 1))
      p = &mcpprompts[i];
  }
  if (!p) return 0;

  if (!stralloc_copys(&params, "{\"name\":") ||
      !json_str(&params, p->name, str_len(p->name)) ||
      !stralloc_cats(&params, ",\"arguments\":{"))
    nomem();
  for (a = name + n; *a == ' '; ++a) ;
  for (k = 0; k < p->nargs; ++k) {
    if (!*a) {
      if (!p->req[k]) continue;
      kwarn("the prompt needs ", p->args[k], 0);
      outs("(usage: /");
      out(name, n);
      for (k = 0; k < p->nargs; ++k) {
        outs(p->req[k] ? " " : " [");
        outs(p->args[k]);
        if (!p->req[k]) outs("]");
      }
      outs(")\n");
      return 1;
    }
    e = k == p->nargs - 1 ? str_len(a) : byte_chr(a, str_len(a), ' ');
    if ((params.s[params.len - 1] != '{' && !stralloc_cats(&params, ",")) ||
        !json_str(&params, p->args[k], str_len(p->args[k])) ||
        !stralloc_cats(&params, ":") || !json_str(&params, a, e))
      nomem();
    for (a += e; *a == ' '; ++a) ;
  }
  if (!stralloc_cats(&params, "}}") || !stralloc_0(&params)) nomem();

  if (!mcp_rpc(&mcps[p->srv - 1], "prompts/get", params.s, &res, 120, &intr)) {
    if (!stralloc_0(&res)) nomem();
    kwarn("MCP server ", mcps[p->srv - 1].name, ": unable to get the prompt");
    outs("  (");
    outs(res.s);
    outs(")\n");
    return 1;
  }
  if (!stralloc_copys(&text, "")) nomem();
  texts(res.s, res.len, &text);
  if (!text.len) {
    kwarn("the prompt came back empty", 0, 0);
    return 1;
  }
  sgr(DIM);
  outs("(the prompt from MCP server ");
  outs(mcps[p->srv - 1].name);
  outs(")");
  sgr(0);
  outs("\n");
  /* someone else's words: show what will be asked */
  lines("  | ", text.s, text.len, 6);
  status = 0;
  task(text.s, text.len);
  nl();
  flush();
  return 1;
}
