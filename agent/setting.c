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
  if (!str_diff(k, "api")) cfgapi = sdup(v);
  else if (!str_diff(k, "url")) base = sdup(v);
  else if (!str_diff(k, "model")) model = sdup(v);
  else if (!str_diff(k, "check_model")) checkmodel = sdup(v);
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
