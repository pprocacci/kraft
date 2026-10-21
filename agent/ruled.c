#include "agent.h"

static int
forme(const struct rule *r, const char *name)
{
  return !str_diff(r->tool, "*") || !str_diff(r->tool, name) ||
      (!str_diff(r->tool, "edit") && isedit(name));
}

/* what the rules say about tool t with input in: 1 allow, -1 deny, 0 */
int
ruled(const struct tool *t, const char *in, size_t n, const struct rule **why)
{
  static stralloc subj, arg;
  unsigned long i;
  int bash = !str_diff(t->name, "bash"), canallow = 1;

  /* what the patterns are matched against */
  if (!stralloc_copys(&subj, "")) nomem();
  if (bash) {
    if (!tool_arg(in, n, "command", &arg)) canallow = 0;
    else if (!stralloc_copy(&subj, &arg)) nomem();
  } else if (isedit(t->name) || !str_diff(t->name, "read_file")) {
    if (!tool_arg(in, n, "path", &arg)) canallow = 0;
    else {
      relpath(arg.s, &subj);
      subj.len = str_len(subj.s);
    }
  } else if (!str_diff(t->name, "web_fetch")) {
    if (!tool_arg(in, n, "url", &arg)) canallow = 0;
    else if (!stralloc_copy(&subj, &arg)) nomem();
  }
  if (!stralloc_0(&subj)) nomem();
  if (bash && !plain(subj.s)) canallow = 0;
  /* the model must not grant itself permissions */
  if (isedit(t->name) && (str_start(subj.s, ".kraft/") ||
      hits("*/.kraft/*", subj.s, 0)))
    canallow = 0;

  for (i = 0; i < nrules; ++i)
    if (!rules[i].allow && forme(&rules[i], t->name) &&
        hits(rules[i].pat, subj.s, bash)) {
      *why = &rules[i];
      return -1;
    }
  for (i = 0; canallow && i < nrules; ++i)
    if (rules[i].allow && forme(&rules[i], t->name) &&
        hits(rules[i].pat, subj.s, 0)) {
      *why = &rules[i];
      return 1;
    }
  return 0;
}
