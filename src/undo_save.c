#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "file.h"
#include "str.h"
#include "stralloc.h"
#include "undo.h"

/* the absolute path of a file, which need not exist yet */
static int
absolute(const char *path, stralloc *out)
{
  char real[PATH_MAX];
  stralloc d = STRALLOC_ZERO;
  size_t n;
  int ok = 0;

  if (realpath(path, real)) return stralloc_copys(out, real);
  n = str_len(path);
  while (n && path[n - 1] != '/') --n;
  if (!stralloc_copyb(&d, n ? path : ".", n ? n : 1) || !stralloc_0(&d))
    goto out;
  if (!realpath(d.s, real)) goto out;
  ok = stralloc_copys(out, real) && stralloc_cats(out, "/") &&
      stralloc_cats(out, path + n);
out:
  stralloc_free(&d);
  return ok;
}

/*
 * saves entry seq in dir (made if need be): task, whether path exists,
 * its absolute path, and then its contents.  returns 1, or 0 with
 * errno set.
 */
int
undo_save(const char *dir, unsigned long seq, unsigned long task,
    const char *path)
{
  stralloc e = STRALLOC_ZERO, f = STRALLOC_ZERO, fn = STRALLOC_ZERO;
  stralloc ap = STRALLOC_ZERO;
  int existed, fd, ok = 0;

  mkdir(dir, 0700);
  existed = file_get(path, &f);
  if (!existed && errno != ENOENT) goto out;
  if (!absolute(path, &ap)) goto out;
  if (!stralloc_copys(&e, "") || !stralloc_catulong(&e, task) ||
      !stralloc_cats(&e, existed ? "\n1\n" : "\n0\n") ||
      !stralloc_cat(&e, &ap) || !stralloc_cats(&e, "\n") ||
      !stralloc_cat(&e, &f))
    goto out;
  if (!stralloc_copys(&fn, dir) || !stralloc_cats(&fn, "/") ||
      !stralloc_catulong(&fn, seq) || !stralloc_0(&fn))
    goto out;
  /* private from the start; file_put keeps the mode */
  if ((fd = open(fn.s, O_WRONLY | O_CREAT | O_CLOEXEC, 0600)) != -1) close(fd);
  ok = file_put(fn.s, e.s, e.len);
out:
  stralloc_free(&e);
  stralloc_free(&f);
  stralloc_free(&fn);
  stralloc_free(&ap);
  return ok;
}
