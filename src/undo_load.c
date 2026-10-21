#include "byte.h"
#include "file.h"
#include "scan.h"
#include "stralloc.h"
#include "undo.h"

/*
 * reads entry seq in dir: its task, the file's path (NUL-terminated),
 * whether it existed, and its contents then.  returns 1, or 0 if
 * there is no such entry or it is malformed.
 */
int
undo_load(const char *dir, unsigned long seq, unsigned long *task,
    stralloc *path, stralloc *data, int *existed)
{
  stralloc fn = STRALLOC_ZERO, e = STRALLOC_ZERO;
  size_t i, j, k;
  int ok = 0;

  if (!stralloc_copys(&fn, dir) || !stralloc_cats(&fn, "/") ||
      !stralloc_catulong(&fn, seq) || !stralloc_0(&fn))
    goto out;
  if (!file_get(fn.s, &e)) goto out;
  i = byte_chr(e.s, e.len, '\n');
  if (i == e.len || scan_ulong(e.s, task) != i) goto out;
  j = i + 1 + byte_chr(e.s + i + 1, e.len - i - 1, '\n');
  if (j != i + 2 || (e.s[i + 1] != '0' && e.s[i + 1] != '1')) goto out;
  *existed = e.s[i + 1] == '1';
  k = j + 1 + byte_chr(e.s + j + 1, e.len - j - 1, '\n');
  if (k >= e.len) goto out;
  if (!stralloc_copyb(path, e.s + j + 1, k - j - 1) || !stralloc_0(path))
    goto out;
  --path->len;
  if (!stralloc_copyb(data, e.s + k + 1, e.len - k - 1)) goto out;
  ok = 1;
out:
  stralloc_free(&fn);
  stralloc_free(&e);
  return ok;
}
