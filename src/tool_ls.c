#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "alloc.h"
#include "path.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

#define MAXENTRIES 1000

static int
cmp(const void *a, const void *b)
{
  return str_diff(*(char *const *)a, *(char *const *)b);
}

/* list_dir: optional path */
int
tool_ls(const char *in, size_t n, stralloc *out)
{
  stralloc dir = STRALLOC_ZERO, names = STRALLOC_ZERO, full = STRALLOC_ZERO;
  struct dirent *d;
  struct stat st;
  DIR *dp;
  char **v = 0;
  size_t i, off, count = 0;
  int isdir, ok = 0;

  if (!tool_arg(in, n, "path", &dir) || !dir.len)
    if (!stralloc_copys(&dir, ".") || !stralloc_0(&dir)) goto out;
  if (path_secret(dir.s)) {
    stralloc_copys(out, "refused: ");
    stralloc_cats(out, dir.s);
    stralloc_cats(out, " is kraft's private directory");
    goto out;
  }
  if (!(dp = opendir(dir.s))) {
    stralloc_copys(out, "unable to list ");
    stralloc_cats(out, dir.s);
    stralloc_cats(out, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  /* names, each NUL-terminated, directories with a trailing slash */
  while ((d = readdir(dp))) {
    if (!str_diff(d->d_name, ".") || !str_diff(d->d_name, "..")) continue;
    isdir = d->d_type == DT_DIR;
    if (d->d_type == DT_UNKNOWN) {
      if (!stralloc_copys(&full, dir.s) || !stralloc_cats(&full, "/") ||
          !stralloc_cats(&full, d->d_name) || !stralloc_0(&full))
        break;
      isdir = stat(full.s, &st) == 0 && S_ISDIR(st.st_mode);
    }
    if (!stralloc_cats(&names, d->d_name)) break;
    if (isdir && !stralloc_cats(&names, "/")) break;
    if (!stralloc_0(&names)) break;
    ++count;
  }
  closedir(dp);

  if (count && !(v = alloc(count * sizeof *v))) goto out;
  for (i = 0, off = 0; i < count; ++i) {
    v[i] = names.s + off;
    off += str_len(v[i]) + 1;
  }
  qsort(v, count, sizeof *v, cmp);
  stralloc_copys(out, "");
  for (i = 0; i < count && i < MAXENTRIES; ++i) {
    stralloc_cats(out, v[i]);
    stralloc_append(out, '\n');
  }
  if (!count) stralloc_cats(out, "(empty directory)");
  if (count > MAXENTRIES) {
    stralloc_cats(out, "[");
    stralloc_catulong(out, (unsigned long)(count - MAXENTRIES));
    stralloc_cats(out, " more entries not shown]");
  }
  ok = 1;
out:
  alloc_free(v);
  stralloc_free(&dir);
  stralloc_free(&names);
  stralloc_free(&full);
  return ok;
}
