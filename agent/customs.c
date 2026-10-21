#include "agent.h"

static void
list(const char *dir, const char *label)
{
  stralloc fn = STRALLOC_ZERO, f = STRALLOC_ZERO;
  struct dirent *d;
  DIR *dp;
  size_t n, e;

  if (!(dp = opendir(dir))) return;
  while ((d = readdir(dp))) {
    n = str_len(d->d_name);
    if (n < 4 || str_diff(d->d_name + n - 3, ".md")) continue;
    if (!stralloc_copys(&fn, dir) || !stralloc_cats(&fn, "/") ||
        !stralloc_cats(&fn, d->d_name) || !stralloc_0(&fn))
      nomem();
    if (!file_get(fn.s, &f)) continue;
    outs("  /");
    out(d->d_name, n - 3);
    for (e = n - 3; e < 9; ++e) outs(" ");
    outs(" ");
    /* its first line says what it is for */
    e = byte_chr(f.s, f.len, '\n');
    sgr(DIM);
    out(f.s, e > 60 ? 60 : e);
    if (e > 60) outs("...");
    outs(label);
    sgr(0);
    outs("\n");
  }
  closedir(dp);
  stralloc_free(&fn);
  stralloc_free(&f);
}

/* lists the commands the user has written, for /help */
void
customs(void)
{
  stralloc d = STRALLOC_ZERO;

  list(".kraft/commands", " (this project)");
  if (!stralloc_copys(&d, env("HOME", "")) ||
      !stralloc_cats(&d, "/.kraft/commands") || !stralloc_0(&d))
    nomem();
  list(d.s, "");
  stralloc_free(&d);
}
