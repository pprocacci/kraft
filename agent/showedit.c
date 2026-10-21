#include "agent.h"
#include "diff.h"
#include "edit.h"

#define SHOWN 60  /* the most lines of a diff to show */

/* the file tool t would leave, from its input in; 0 if it would fail */
static int
after(const struct tool *t, const char *in, size_t n, const stralloc *was,
    stralloc *will)
{
  static stralloc old, new, key, err;
  const char *e;
  size_t en;
  unsigned long k;

  if (!str_diff(t->name, "write_file"))
    return tool_arg(in, n, "content", will);
  if (!stralloc_copy(will, was)) nomem();
  if (!str_diff(t->name, "edit_file"))
    return tool_arg(in, n, "old_string", &old) &&
        tool_arg(in, n, "new_string", &new) &&
        edit_apply(will, &old, &new, tool_argflag(in, n, "replace_all"), &err);
  for (k = 0;; ++k) {
    if (!stralloc_copys(&key, "edits.") || !stralloc_catulong(&key, k) ||
        !stralloc_0(&key))
      nomem();
    if (!json_get(in, n, key.s, &e, &en)) break;
    if (!tool_arg(e, en, "old_string", &old) ||
        !tool_arg(e, en, "new_string", &new) ||
        !edit_apply(will, &old, &new, tool_argflag(e, en, "replace_all"), &err))
      return 0;
  }
  return k > 0;
}

/*
 * shows the change a file tool is about to make, as a diff against
 * the file as it is.  shows nothing if the call would fail anyway.
 */
void
showedit(const struct tool *t, const char *in, size_t n)
{
  static stralloc p, was, will, d;
  unsigned long shown = 0, more = 0, plus = 0, minus = 0;
  size_t i, e, len;
  int isnew;

  if (!tool_arg(in, n, "path", &p) || path_secret(p.s)) return;
  isnew = !file_get(p.s, &was);
  if (isnew && (errno != ENOENT || !stralloc_copys(&was, ""))) return;
  if (!after(t, in, n, &was, &will)) return;
  if (!stralloc_copys(&d, "") ||
      !diff_lines(&d, was.s, was.len, will.s, will.len, 3))
    nomem();
  if (!d.len) {
    styled(DIM, "  (no change)");
    outs("\n");
    return;
  }
  for (i = 0; i < d.len; i = e + 1) {
    e = i + byte_chr(d.s + i, d.len - i, '\n');
    if (d.s[i] == '+') ++plus;
    if (d.s[i] == '-') ++minus;
    if (shown == SHOWN) { ++more; continue; }
    ++shown;
    len = e - i - 1;
    if (len > 200) len = 200;
    if (d.s[i] == '@') {
      sgr(DIM);
      outs("  @@ line ");
      out(d.s + i + 3, e - i - 3);
    } else {
      sgr(d.s[i] == '+' ? GREEN : d.s[i] == '-' ? RED : DIM);
      outs(d.s[i] == '+' ? "  + " : d.s[i] == '-' ? "  - " : "    ");
      out(d.s + i + 1, len);
      if (e - i - 1 > 200) outs("...");
    }
    sgr(0);
    outs("\n");
  }
  sgr(DIM);
  if (more) {
    if (!stralloc_copys(&p, "  ... (") || !stralloc_catulong(&p, more) ||
        !stralloc_cats(&p, " more lines of diff)\n") || !stralloc_0(&p))
      nomem();
    outs(p.s);
  }
  if (!stralloc_copys(&p, isnew ? "  (a new file, " : "  (") ||
      !stralloc_cats(&p, "+") || !stralloc_catulong(&p, plus) ||
      !stralloc_cats(&p, " -") || !stralloc_catulong(&p, minus) ||
      !stralloc_cats(&p, ")") || !stralloc_0(&p))
    nomem();
  outs(p.s);
  sgr(0);
  outs("\n");
}
