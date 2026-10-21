#include <errno.h>
#include <string.h>
#include "edit.h"
#include "file.h"
#include "json.h"
#include "path.h"
#include "stralloc.h"
#include "tool.h"

/*
 * multi_edit: path, edits (an array of old_string, new_string,
 * replace_all).  the edits are applied in order to the file in memory;
 * it is written only if every one of them succeeds.
 */
int
tool_multiedit(const char *in, size_t n, stralloc *out)
{
  stralloc path = STRALLOC_ZERO, old = STRALLOC_ZERO, new = STRALLOC_ZERO;
  stralloc f = STRALLOC_ZERO, err = STRALLOC_ZERO, key = STRALLOC_ZERO;
  unsigned long k, count, total = 0;
  const char *e;
  size_t en;
  int ok = 0;

  if (!tool_arg(in, n, "path", &path)) {
    stralloc_copys(out, "path is required");
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
  for (k = 0;; ++k) {
    if (!stralloc_copys(&key, "edits.") || !stralloc_catulong(&key, k) ||
        !stralloc_0(&key))
      goto nomem;
    if (!json_get(in, n, key.s, &e, &en)) break;
    if (!tool_arg(e, en, "old_string", &old) ||
        !tool_arg(e, en, "new_string", &new)) {
      stralloc_copys(out, "edit ");
      stralloc_catulong(out, k + 1);
      stralloc_cats(out, " needs old_string and new_string; nothing was "
          "changed");
      goto out;
    }
    count = edit_apply(&f, &old, &new, tool_argflag(e, en, "replace_all"),
        &err);
    if (!count) {
      stralloc_copys(out, "edit ");
      stralloc_catulong(out, k + 1);
      stralloc_cats(out, " failed: ");
      stralloc_cat(out, &err);
      stralloc_cats(out, ". Nothing was changed in ");
      stralloc_cats(out, path.s);
      goto out;
    }
    total += count;
  }
  if (!k) {
    stralloc_copys(out, "edits must be a non-empty array");
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
  stralloc_catulong(out, k);
  stralloc_cats(out, k == 1 ? " edit, " : " edits, ");
  stralloc_catulong(out, total);
  stralloc_cats(out, total == 1 ? " replacement" : " replacements");
  ok = 1;
  goto out;
nomem:
  stralloc_copys(out, "out of memory");
out:
  stralloc_free(&path);
  stralloc_free(&old);
  stralloc_free(&new);
  stralloc_free(&f);
  stralloc_free(&err);
  stralloc_free(&key);
  return ok;
}
