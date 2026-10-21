#include "agent.h"

/* asks whether tool t may run with input in; 1 if allowed */
int
approve(const struct tool *t, const char *in, size_t n)
{
  const struct rule *ru;
  size_t i;
  int c, r;

  /* the user's rules first: a deny holds whatever the mode */
  if (!stralloc_copys(&ruledeny, "")) nomem();
  if ((r = ruled(t, in, n, &ru)) < 0) {
    if (!stralloc_copys(&ruledeny, "refused by the user's rule \"deny ") ||
        !stralloc_cats(&ruledeny, ru->tool) || !stralloc_cats(&ruledeny, " ") ||
        !stralloc_cats(&ruledeny, ru->pat) || !stralloc_cats(&ruledeny, "\" in ") ||
        !stralloc_cats(&ruledeny, ru->from) || !stralloc_0(&ruledeny))
      nomem();
    sgr(RED);
    outs("  (");
    outs(ruledeny.s);
    outs(")");
    sgr(0);
    outs("\n");
    return 0;
  }
  if (!t->ask) return 1;
  if (r > 0) {
    sgr(DIM);
    outs("  (allowed by the rule \"allow ");
    outs(ru->tool);
    outs(" ");
    outs(ru->pat);
    outs("\")");
    sgr(0);
    outs("\n");
    return 1;
  }

  /* by name: t may be from a sub-agent's list */
  for (i = 0; tools[i].name && str_diff(tools[i].name, t->name); ++i) ;
  if (mode == MODE_YES || allowed[i]) return 1;
  if (mode == MODE_RULES || mode == MODE_AUTO) {
    r = perm_auto(t->name, in, n, &rwhy);
    if (r == PERM_ALLOW) return 1;
    if (!stralloc_0(&rwhy)) nomem();
    if (r == PERM_UNSURE && mode == MODE_AUTO) {
      if (check(t, in, n)) {
        sgr(DIM);
        outs("  (allowed by the auto-mode check: ");
        outs(cwhy.s);
        outs(")");
        sgr(0);
        outs("\n");
        return 1;
      }
      if (intr) return 0;
      sgr(YELLOW);
      outs("  (auto mode asks: ");
      outs(rwhy.s);
      outs("; the check said: ");
      outs(cwhy.s);
      outs(")");
    } else {
      sgr(YELLOW);
      outs(mode == MODE_RULES ? "  (the safety rules ask: " : "  (auto mode asks: ");
      outs(rwhy.s);
      outs(")");
    }
    sgr(0);
    outs("\n");
  }
  if (!str_diff(t->name, "bash")) {
    if (tool_arg(in, n, "command", &tmp)) lines("  $ ", tmp.s, tmp.len, 20);
  } else if (!str_diff(t->name, "edit_file")) {
    if (tool_arg(in, n, "old_string", &tmp)) lines("  - ", tmp.s, tmp.len, 20);
    if (tool_arg(in, n, "new_string", &tmp)) lines("  + ", tmp.s, tmp.len, 20);
  } else if (!str_diff(t->name, "write_file")) {
    if (tool_arg(in, n, "content", &tmp)) lines("  + ", tmp.s, tmp.len, 20);
  } else if (!str_diff(t->name, "multi_edit")) {
    static stralloc key;
    const char *e;
    size_t en;
    unsigned long k;

    for (k = 0; k < 8; ++k) {
      if (!stralloc_copys(&key, "edits.") || !stralloc_catulong(&key, k) ||
          !stralloc_0(&key))
        nomem();
      if (!json_get(in, n, key.s, &e, &en)) break;
      if (tool_arg(e, en, "old_string", &tmp)) lines("  - ", tmp.s, tmp.len, 8);
      if (tool_arg(e, en, "new_string", &tmp)) lines("  + ", tmp.s, tmp.len, 8);
    }
  } else if (!str_diff(t->name, "web_fetch")) {
    if (tool_arg(in, n, "url", &tmp)) lines("  GET ", tmp.s, tmp.len, 1);
  }
  flush();
  c = ui_ask(color ? "  \033[1;33mAllow?\033[0m [y]es, [n]o, [a]lways this "
      "session, [p]ermanently here: " : "  Allow? [y]es, [n]o, [a]lways this "
      "session, [p]ermanently here: ");
  last = '\n';
  if (!c) {
    styled(YELLOW, "  (declined: no terminal to ask on; see -P)");
    outs("\n");
    return 0;
  }
  if (c == 'a') allowed[i] = 1;
  if (c == 'p') saverule(t, in, n);
  return c != 'n';
}
