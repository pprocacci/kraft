#include "agent.h"

#define HOOKSECS 120
#define MAXENV 65536

/* sets name to s (n bytes, cut short if need be) for the hooks */
static void
put(const char *name, const char *s, size_t n)
{
  static stralloc v;

  if (n > MAXENV) n = MAXENV;
  if (!stralloc_copyb(&v, s, n) || !stralloc_0(&v)) nomem();
  /* a NUL inside would only cut it short */
  setenv(name, v.s, 1);
}

/*
 * runs the hooks for when (HOOK_BEFORE, HOOK_AFTER) and tool t, with
 * input in, or (HOOK_DONE, t 0) at the end of a task.  the call's
 * details are in the environment: KRAFT_TOOL, KRAFT_INPUT, KRAFT_PATH
 * and KRAFT_COMMAND (when the input has them), and, after it,
 * KRAFT_RESULT and KRAFT_FAILED.  what the hooks print goes in out.
 * returns 0 if a "before" hook failed, so the call must not be made.
 */
int
hook(int when, const struct tool *t, const char *in, size_t n,
    const stralloc *result, int failed, stralloc *out)
{
  static stralloc o, a;
  char *argv[4];
  unsigned long i;
  int st, ok = 1, set = 0;

  if (!stralloc_copys(out, "")) nomem();
  for (i = 0; i < nhooks && ok; ++i) {
    if (hooks[i].when != when) continue;
    if (t && !(!fnmatch(hooks[i].tool, t->name, 0) ||
        (!str_diff(hooks[i].tool, "edit") && isedit(t->name))))
      continue;
    if (!set && !t) {
      setenv("KRAFT_FAILED", failed ? "1" : "0", 1);
      set = 1;
    }
    if (!set) {
      setenv("KRAFT_TOOL", t->name, 1);
      put("KRAFT_INPUT", in, n);
      if (tool_arg(in, n, "path", &a)) put("KRAFT_PATH", a.s, a.len);
      else unsetenv("KRAFT_PATH");
      if (tool_arg(in, n, "command", &a)) put("KRAFT_COMMAND", a.s, a.len);
      else unsetenv("KRAFT_COMMAND");
      if (result) put("KRAFT_RESULT", result->s, result->len);
      setenv("KRAFT_FAILED", failed ? "1" : "0", 1);
      set = 1;
    }
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = (char *)hooks[i].cmd;
    argv[3] = 0;
    if (!stralloc_copys(&o, "")) nomem();
    if (!proc_run(argv, HOOKSECS, 65536, &o, &st)) {
      kwarnsys("unable to run a hook: ", hooks[i].cmd);
      continue;
    }
    while (o.len && o.s[o.len - 1] == '\n') --o.len;
    if (st && !stralloc_copys(&a, "[hook \"") ) nomem();
    if (st && (!stralloc_cats(&a, hooks[i].cmd) ||
        !stralloc_cats(&a, st < 0 ? "\" timed out" : "\" failed, status ") ||
        (st > 0 && !stralloc_catulong(&a, (unsigned long)st)) ||
        !stralloc_cats(&a, "]")))
      nomem();
    if (out->len && (o.len || st) && !stralloc_append(out, '\n')) nomem();
    if (st && !stralloc_cat(out, &a)) nomem();
    if (st && o.len && !stralloc_append(out, '\n')) nomem();
    if (!stralloc_cat(out, &o)) nomem();
    if (st && when == HOOK_BEFORE) ok = 0;
  }
  if (set) {
    unsetenv("KRAFT_TOOL");
    unsetenv("KRAFT_INPUT");
    unsetenv("KRAFT_PATH");
    unsetenv("KRAFT_COMMAND");
    unsetenv("KRAFT_RESULT");
    unsetenv("KRAFT_FAILED");
  }
  return ok;
}
