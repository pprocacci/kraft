#include "agent.h"

static int
yesno(const char *v)
{
  if (!str_diff(v, "yes") || !str_diff(v, "on") || !str_diff(v, "true") ||
      !str_diff(v, "1"))
    return 1;
  if (!str_diff(v, "no") || !str_diff(v, "off") || !str_diff(v, "false") ||
      !str_diff(v, "0"))
    return 0;
  return -1;
}

int
setting(void *arg, const char *k, const char *v, unsigned long line)
{
  unsigned long u;
  size_t i;
  int y;

  (void)arg;
  if (!stralloc_copys(&tmp, cfgfile) || !stralloc_cats(&tmp, ":") ||
      !stralloc_catulong(&tmp, line) || !stralloc_cats(&tmp, ": ") ||
      !stralloc_0(&tmp))
    nomem();
  if (!str_diff(k, "allow") || !str_diff(k, "deny")) {
    if (nrules == sizeof rules / sizeof rules[0]) {
      kwarn(tmp.s, "too many rules", 0);
      return 1;
    }
    i = byte_chr(v, str_len(v), ' ');
    rules[nrules].allow = !str_diff(k, "allow");
    rules[nrules].from = cfgfile;
    if (!stralloc_copyb(&word, v, i) || !stralloc_0(&word)) nomem();
    rules[nrules].tool = sdup(word.s);
    while (v[i] == ' ' || v[i] == '\t') ++i;
    rules[nrules].pat = sdup(v[i] ? v + i : "*");
    if (!*rules[nrules].tool) { kwarn(tmp.s, k, " needs a tool"); return 1; }
    ++nrules;
    return 1;
  }
  if (!str_diff(k, "hook")) {
    /* hook before|after TOOL COMMAND, or hook done COMMAND */
    i = byte_chr(v, str_len(v), ' ');
    if (!stralloc_copyb(&word, v, i) || !stralloc_0(&word)) nomem();
    y = !str_diff(word.s, "before") ? HOOK_BEFORE :
        !str_diff(word.s, "after") ? HOOK_AFTER :
        !str_diff(word.s, "done") ? HOOK_DONE : 0;
    if (!y) {
      kwarn(tmp.s, "a hook is for before, after or done", 0);
      return 1;
    }
    if (nhooks == MAXHOOKS) {
      kwarn(tmp.s, "too many hooks", 0);
      return 1;
    }
    while (v[i] == ' ' || v[i] == '\t') ++i;
    hooks[nhooks].tool = 0;
    if (y != HOOK_DONE) {
      v += i;
      i = byte_chr(v, str_len(v), ' ');
      if (!stralloc_copyb(&word, v, i) || !stralloc_0(&word)) nomem();
      hooks[nhooks].tool = sdup(word.s);
      while (v[i] == ' ' || v[i] == '\t') ++i;
    }
    if (!v[i] || (hooks[nhooks].tool && !*hooks[nhooks].tool)) {
      kwarn(tmp.s, y == HOOK_DONE ? "hook done needs a command" :
          "the hook needs a tool and a command", 0);
      return 1;
    }
    hooks[nhooks].when = y;
    hooks[nhooks].cmd = sdup(v + i);
    hooks[nhooks].from = cfgfile;
    ++nhooks;
    return 1;
  }
  if (!str_diff(k, "mcp_header")) {
    /* mcp_header NAME Header: value, with ${VAR} from the environment */
    i = byte_chr(v, str_len(v), ' ');
    if (!stralloc_copyb(&word, v, i) || !stralloc_0(&word)) nomem();
    while (v[i] == ' ' || v[i] == '\t') ++i;
    for (u = 0; u < nmcps && str_diff(mcps[u].name, word.s); ++u) ;
    if (u == nmcps) {
      kwarn(tmp.s, "mcp_header for an MCP server not yet named: ", word.s);
      return 1;
    }
    v += i;
    i = byte_chr(v, str_len(v), ':');
    if (!i || !v[i]) {
      kwarn(tmp.s, "mcp_header needs a server and a header", 0);
      return 1;
    }
    if (!stralloc_copys(&word, "")) nomem();
    for (; *v; ++v) {
      i = byte_chr(v, str_len(v), '}');
      if (v[0] == '$' && v[1] == '{' && v[i]) {
        if (!stralloc_copyb(&tmp, v + 2, i - 2) || !stralloc_0(&tmp)) nomem();
        if (!env(tmp.s, 0)) {
          kwarn("mcp_header: ", tmp.s, " is not set");
          return 1;
        }
        if (!stralloc_cats(&word, env(tmp.s, 0))) nomem();
        v += i;
      } else if (!stralloc_append(&word, *v)) nomem();
    }
    /* one header line, whatever the environment held */
    for (i = 0; i < word.len; ++i)
      if (word.s[i] == '\r' || word.s[i] == '\n') word.s[i] = ' ';
    if (!stralloc_cat(&mcps[u].hdrs, &word) ||
        !stralloc_cats(&mcps[u].hdrs, "\r\n"))
      nomem();
    return 1;
  }
  if (!str_diff(k, "mcp")) {
    /* mcp NAME COMMAND ... */
    i = byte_chr(v, str_len(v), ' ');
    if (!stralloc_copyb(&word, v, i) || !stralloc_0(&word)) nomem();
    while (v[i] == ' ' || v[i] == '\t') ++i;
    if (!*word.s || !v[i]) {
      kwarn(tmp.s, "mcp needs a name and a command", 0);
      return 1;
    }
    for (u = 0; u < nmcps && str_diff(mcps[u].name, word.s); ++u) ;
    if (u == MAXMCP) {
      kwarn(tmp.s, "too many MCP servers", 0);
      return 1;
    }
    if (u == nmcps) mcps[nmcps++].name = sdup(word.s);
    /* the project's may replace the user's, but not take its headers */
    mcps[u].cmd = sdup(v + i);
    mcps[u].hdrs.len = 0;
    return 1;
  }
  if (!str_diff(k, "api")) cfgapi = sdup(v);
  else if (!str_diff(k, "url")) base = sdup(v);
  else if (!str_diff(k, "model")) model = sdup(v);
  else if (!str_diff(k, "check_model")) checkmodel = sdup(v);
  else if (!str_diff(k, "effort")) {
    if (!pickeffort(v))
      kwarn(tmp.s, "effort must be low, medium, high, xhigh, max or default", 0);
  }
  else if (!str_diff(k, "mode")) {
    if (!pickmode(v)) kwarn(tmp.s, "mode must be ask, rules, auto or yes", 0);
  } else if (!str_diff(k, "max_tokens") || !str_diff(k, "context")) {
    if (!*v || scan_ulong(v, &u) != str_len(v) || !u)
      kwarn(tmp.s, k, " must be a number");
    else if (!str_diff(k, "context")) ctxwin = u;
    else max = u;
  } else if (!str_diff(k, "web_search") || !str_diff(k, "show_thinking")) {
    if ((y = yesno(v)) < 0) kwarn(tmp.s, k, " must be yes or no");
    else if (!str_diff(k, "web_search")) websearch = y;
    else showthink = y;
  } else
    kwarn(tmp.s, "unknown setting ", k);
  return 1;
}
