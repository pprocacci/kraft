#include <limits.h>
#include <stdlib.h>
#include "byte.h"
#include "path.h"
#include "str.h"
#include "stralloc.h"

/* resolves path, or its directory if it does not exist yet, into out */
static int
resolve(const char *path, char *out)
{
  stralloc d = STRALLOC_ZERO;
  char dir[PATH_MAX];
  size_t n;
  int ok = 0;

  if (realpath(path, out)) return 1;
  n = str_len(path);
  while (n && path[n - 1] != '/') --n;
  if (!stralloc_copyb(&d, n ? path : ".", n ? n : 1) || !stralloc_0(&d))
    goto out;
  if (!realpath(d.s, dir)) goto out;
  if (str_len(dir) + 1 + str_len(path + n) >= PATH_MAX) goto out;
  if (!stralloc_copys(&d, dir) || !stralloc_cats(&d, "/") ||
      !stralloc_cats(&d, path + n) || !stralloc_0(&d))
    goto out;
  byte_copy(out, d.len, d.s);
  ok = 1;
out:
  stralloc_free(&d);
  return ok;
}

/*
 * 1 if path is, or is inside, ~/.kraft: kraft's keys and saved
 * sessions, which the model must not read or change.  a path whose
 * directory does not exist is not secret: ~/.kraft does exist, so it
 * cannot lead there, and using it will fail anyway.
 */
int
path_secret(const char *path)
{
  stralloc k = STRALLOC_ZERO;
  char real[PATH_MAX], kr[PATH_MAX];
  const char *home = getenv("HOME");
  size_t n;
  int r = 0;

  if (!home || !*home) return 0;
  if (!stralloc_copys(&k, home) || !stralloc_cats(&k, "/.kraft") ||
      !stralloc_0(&k)) {
    r = 1;
    goto out;
  }
  if (!realpath(k.s, kr)) goto out;  /* no ~/.kraft, nothing to protect */
  if (!resolve(path, real)) goto out;
  n = str_len(kr);
  r = str_start(real, kr) && (real[n] == '/' || real[n] == 0);
out:
  stralloc_free(&k);
  return r;
}
