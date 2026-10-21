#include <errno.h>
#include <string.h>
#include "byte.h"
#include "file.h"
#include "path.h"
#include "stralloc.h"
#include "tool.h"

/* the index of the next occurrence of t in s at or after i, or n */
static size_t
find(const char *s, size_t n, size_t i, const char *t, size_t tn)
{
  for (; i + tn <= n; ++i)
    if (s[i] == t[0] && !byte_diff(s + i, tn, t)) return i;
  return n;
}

/*
 * edit_file: path, old_string, new_string, replace_all.  refuses to
 * change anything unless old_string occurs exactly once (or at least
 * once, with replace_all).
 */
int
tool_edit(const char *in, size_t n, stralloc *out)
{
  stralloc path = STRALLOC_ZERO, old = STRALLOC_ZERO, new = STRALLOC_ZERO;
  stralloc f = STRALLOC_ZERO, g = STRALLOC_ZERO;
  unsigned long count = 0;
  size_t i, j;
  int all, ok = 0;

  if (!tool_arg(in, n, "path", &path) || !tool_arg(in, n, "old_string", &old) ||
      !tool_arg(in, n, "new_string", &new)) {
    stralloc_copys(out, "path, old_string and new_string are required");
    goto out;
  }
  all = tool_argflag(in, n, "replace_all");
  if (!old.len) {
    stralloc_copys(out, "old_string is empty; use write_file to create a file");
    goto out;
  }
  if (old.len == new.len && !byte_diff(old.s, old.len, new.s)) {
    stralloc_copys(out, "old_string and new_string are the same");
    goto out;
  }
  if (path_secret(path.s)) {
    stralloc_copys(out, "refused: ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, " is one of kraft's private files");
    goto out;
  }
  if (!file_get(path.s, &f)) {
    stralloc_copys(out, "unable to read ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }

  for (i = 0; (i = find(f.s, f.len, i, old.s, old.len)) < f.len; i += old.len)
    ++count;
  if (!count) {
    stralloc_copys(out, "old_string was not found in ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, "; read the file again and copy the text exactly");
    goto out;
  }
  if (count > 1 && !all) {
    stralloc_copys(out, "old_string occurs ");
    stralloc_catulong(out, count);
    stralloc_cats(out, " times in ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, "; add surrounding lines to make it unique, or set "
        "replace_all");
    goto out;
  }

  if (!stralloc_copys(&g, "")) goto nomem;
  for (i = 0; (j = find(f.s, f.len, i, old.s, old.len)) < f.len;
      i = j + old.len) {
    if (!stralloc_catb(&g, f.s + i, j - i)) goto nomem;
    if (!stralloc_catb(&g, new.s, new.len)) goto nomem;
  }
  if (!stralloc_catb(&g, f.s + i, f.len - i)) goto nomem;
  if (!file_put(path.s, g.s, g.len)) {
    stralloc_copys(out, "unable to write ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  stralloc_copys(out, "edited ");
  stralloc_cats(out, path.s);
  stralloc_cats(out, ": ");
  stralloc_catulong(out, count);
  stralloc_cats(out, count == 1 ? " replacement" : " replacements");
  ok = 1;
  goto out;
nomem:
  stralloc_copys(out, "out of memory");
out:
  stralloc_free(&path);
  stralloc_free(&old);
  stralloc_free(&new);
  stralloc_free(&f);
  stralloc_free(&g);
  return ok;
}
