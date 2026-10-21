#include "agent.h"

/* "* name(what)": a one-line description of a tool call */
void
announce(const char *prefix, const struct block *b, const char *in, size_t n)
{
  static const char *keys[] = {
    "command", "path", "pattern", "url", "description", 0
  };
  unsigned long u;
  size_t i, k;

  nl();
  if (color) {
    /* a dot for a call, an arrow for a sub-agent's */
    sgr(*prefix == '*' ? CYAN : DIM);
    outs(*prefix == '*' ? "\342\227\217 " : "  \342\206\263 ");
    sgr(*prefix == '*' ? TOOLC : DIM);
    out(b->name.s, b->name.len);
    sgr(0);
    if (*prefix != '*') sgr(DIM);
  } else {
    outs(prefix);
    out(b->name.s, b->name.len);
  }
  outs("(");
  for (i = 0; keys[i]; ++i)
    if (tool_arg(in, n, keys[i], &tmp)) break;
  if (keys[i]) {
    for (k = 0; k < tmp.len && k < 100; ++k)
      out(tmp.s[k] == '\n' ? " " : tmp.s + k, 1);
    if (tmp.len > 100) outs("...");
  } else if (tool_argnum(in, n, "id", &u)) {
    if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, u) ||
        !stralloc_0(&tmp))
      nomem();
    outs(tmp.s);
  } else if (tool_arg(in, n, "question", &tmp))
    out(tmp.s, tmp.len > 100 ? 100 : tmp.len);
  outs(")");
  sgr(0);
  outs("\n");
}
