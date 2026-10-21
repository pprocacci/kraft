#include "json.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

/*
 * todo: todos, an array of content and status (pending, in_progress
 * or completed).  the whole list comes each time; it is shown as a
 * checklist, and that is also what the model gets back.
 */
int
tool_todo(const char *in, size_t n, stralloc *out)
{
  stralloc key = STRALLOC_ZERO, what = STRALLOC_ZERO, st = STRALLOC_ZERO;
  unsigned long k, done = 0;
  const char *e;
  size_t en;
  int ok = 0;

  if (!stralloc_copys(out, "")) goto out;
  for (k = 0;; ++k) {
    if (!stralloc_copys(&key, "todos.") || !stralloc_catulong(&key, k) ||
        !stralloc_0(&key))
      goto out;
    if (!json_get(in, n, key.s, &e, &en)) break;
    if (!tool_arg(e, en, "content", &what) || !tool_arg(e, en, "status", &st)) {
      stralloc_copys(out, "each item needs content and status");
      goto out;
    }
    if (!stralloc_0(&st)) goto out;
    if (!str_diff(st.s, "completed")) { stralloc_cats(out, "[x] "); ++done; }
    else if (!str_diff(st.s, "in_progress")) stralloc_cats(out, "[>] ");
    else if (!str_diff(st.s, "pending")) stralloc_cats(out, "[ ] ");
    else {
      stralloc_copys(out, "status must be pending, in_progress or completed");
      goto out;
    }
    stralloc_cat(out, &what);
    stralloc_append(out, '\n');
  }
  if (!k) stralloc_cats(out, "(the list is empty)\n");
  else {
    stralloc_catulong(out, done);
    stralloc_cats(out, " of ");
    stralloc_catulong(out, k);
    stralloc_cats(out, " done\n");
  }
  ok = 1;
out:
  stralloc_free(&key);
  stralloc_free(&what);
  stralloc_free(&st);
  return ok;
}
