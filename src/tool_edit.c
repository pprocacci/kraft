#include <errno.h>
#include <string.h>
#include "edit.h"
#include "file.h"
#include "path.h"
#include "stralloc.h"
#include "tool.h"

/*
 * edit_file: path, old_string, new_string, replace_all.  refuses to
 * change anything unless old_string occurs exactly once (or at least
 * once, with replace_all).
 */
int
tool_edit(const char *in, size_t n, stralloc *out)
{
  stralloc path = STRALLOC_ZERO, old = STRALLOC_ZERO, new = STRALLOC_ZERO;
  stralloc f = STRALLOC_ZERO, err = STRALLOC_ZERO;
  unsigned long count;
  int ok = 0;

  if (!tool_arg(in, n, "path", &path) || !tool_arg(in, n, "old_string", &old) ||
      !tool_arg(in, n, "new_string", &new)) {
    stralloc_copys(out, "path, old_string and new_string are required");
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
  count = edit_apply(&f, &old, &new, tool_argflag(in, n, "replace_all"), &err);
  if (!count) {
    stralloc_copy(out, &err);
    stralloc_cats(out, " (in ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, ")");
    goto out;
  }
  if (!file_put(path.s, f.s, f.len)) {
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
out:
  stralloc_free(&path);
  stralloc_free(&old);
  stralloc_free(&new);
  stralloc_free(&f);
  stralloc_free(&err);
  return ok;
}
