#include "agent.h"

/* adds the file fn, if it has anything in it, to s under a heading */
static void
add(stralloc *s, const char *fn, const char *what)
{
  stralloc f = STRALLOC_ZERO;

  if (file_get(fn, &f) && f.len) {
    if (!stralloc_cats(s, "\n\n") || !stralloc_cats(s, what) ||
        !stralloc_cats(s, " (") || !stralloc_cats(s, fn) ||
        !stralloc_cats(s, "):\n\n") || !stralloc_cat(s, &f))
      nomem();
  }
  stralloc_free(&f);
}

/*
 * the instructions kraft keeps for the model, added to the system
 * prompt s: ~/.kraft/KRAFT.md for every project, then the KRAFT.md
 * files from the top of the project (the git root, or the nearest
 * directory below $HOME) down to the working directory, so that the
 * nearest, most particular, come last.
 */
void
memory(stralloc *s)
{
  stralloc dirs = STRALLOC_ZERO, fn = STRALLOC_ZERO;
  const char *home = env("HOME", 0);
  char cwd[PATH_MAX];
  size_t n[64], k = 0, i, l;
  struct stat st;

  if (home) {
    if (!stralloc_copys(&fn, home) || !stralloc_cats(&fn, "/.kraft/KRAFT.md") ||
        !stralloc_0(&fn))
      nomem();
    add(s, fn.s, "Instructions for every project");
  }
  if (!getcwd(cwd, sizeof cwd)) goto out;

  /* how far up: to a directory with .git in it, but not past $HOME */
  if (!stralloc_copys(&dirs, cwd)) nomem();
  for (l = dirs.len; k < sizeof n / sizeof n[0];) {
    n[k++] = l;
    if (!stralloc_copyb(&fn, dirs.s, l) || !stralloc_cats(&fn, "/.git") ||
        !stralloc_0(&fn))
      nomem();
    if (stat(fn.s, &st) == 0) break;  /* the top of the repository */
    while (l > 1 && dirs.s[l - 1] != '/') --l;
    if (l > 1) --l;
    if (l <= 1 || (home && l == str_len(home) && !byte_diff(dirs.s, l, home)))
      break;
  }
  if (!stralloc_0(&dirs)) nomem();
  /* outermost first */
  for (i = k; i-- > 0;) {
    if (!stralloc_copyb(&fn, dirs.s, n[i]) || !stralloc_cats(&fn, "/KRAFT.md") ||
        !stralloc_0(&fn))
      nomem();
    add(s, fn.s, "Project instructions");
  }
out:
  stralloc_free(&dirs);
  stralloc_free(&fn);
}
