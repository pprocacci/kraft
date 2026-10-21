#include "agent.h"

/* a command's name: letters, digits, - and _ */
static int
okname(const char *s, size_t n)
{
  size_t i;

  if (!n || n > 64) return 0;
  for (i = 0; i < n; ++i)
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') ||
        (s[i] >= '0' && s[i] <= '9') || s[i] == '-' || s[i] == '_'))
      return 0;
  return 1;
}

/* the file for command name in dir ("" for ~/.kraft) into fn */
static void
where(stralloc *fn, int project, const char *name, size_t n)
{
  const char *home = env("HOME", "");

  if (!stralloc_copys(fn, project ? ".kraft/commands/" : home) ||
      (!project && !stralloc_cats(fn, "/.kraft/commands/")) ||
      !stralloc_catb(fn, name, n) || !stralloc_cats(fn, ".md") ||
      !stralloc_0(fn))
    nomem();
}

/*
 * runs line, "/name arguments", if name is a command the user has
 * written: .kraft/commands/name.md in the project, or else
 * ~/.kraft/commands/name.md.  the file is the prompt; $ARGUMENTS in
 * it becomes the arguments, which are added at the end if it has no
 * $ARGUMENTS.  returns 1 if it ran one.
 */
int
custom(const char *line)
{
  stralloc fn = STRALLOC_ZERO, f = STRALLOC_ZERO, p = STRALLOC_ZERO;
  const char *name = line + 1, *args;
  size_t n, i;
  int project, ok = 0, used = 0;

  for (n = 0; name[n] && name[n] != ' '; ++n) ;
  if (!okname(name, n)) return 0;
  for (args = name + n; *args == ' '; ++args) ;
  for (project = 1; project >= 0; --project) {
    where(&fn, project, name, n);
    if (file_get(fn.s, &f)) break;
  }
  if (project < 0) goto out;

  for (i = 0; i < f.len; ++i)
    if (f.len - i >= 10 && !byte_diff(f.s + i, 10, "$ARGUMENTS")) {
      if (!stralloc_cats(&p, args)) nomem();
      i += 9;
      used = 1;
    } else if (!stralloc_append(&p, f.s[i])) nomem();
  if (!used && *args &&
      (!stralloc_cats(&p, "\n\n") || !stralloc_cats(&p, args)))
    nomem();

  sgr(DIM);
  outs("(running ");
  outs(fn.s);
  outs(")");
  sgr(0);
  outs("\n");
  /* a project's commands came with it: show what will be asked */
  if (project) lines("  | ", p.s, p.len, 6);
  status = 0;
  task(p.s, p.len);
  nl();
  flush();
  ok = 1;
out:
  stralloc_free(&fn);
  stralloc_free(&f);
  stralloc_free(&p);
  return ok;
}
