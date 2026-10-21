#include "agent.h"

/* path, relative to the working directory if it is inside it */
void
relpath(const char *p, stralloc *out)
{
  char real[PATH_MAX], cwd[PATH_MAX];
  size_t n;

  if (!stralloc_copys(out, p)) nomem();
  if (!realpath(".", cwd)) goto out;
  if (!realpath(p, real)) {
    /* a new file: resolve its directory */
    n = str_len(p);
    while (n && p[n - 1] != '/') --n;
    if (!stralloc_copyb(out, n ? p : ".", n ? n : 1) || !stralloc_0(out)) nomem();
    if (!realpath(out->s, real)) { stralloc_copys(out, p); goto out; }
    if (!stralloc_copys(out, real) || !stralloc_cats(out, "/") ||
        !stralloc_cats(out, p + n))
      nomem();
  } else if (!stralloc_copys(out, real)) nomem();
  if (!stralloc_0(out)) nomem();
  n = str_len(cwd);
  if (str_start(out->s, cwd) && out->s[n] == '/')
    byte_copy(out->s, str_len(out->s + n + 1) + 1, out->s + n + 1);
out:
  if (!stralloc_0(out)) nomem();
}
