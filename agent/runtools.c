#include "agent.h"

/*
 * runs the tool calls in conversation c's last assistant message,
 * from tool list tl, and puts their results in a new user message.
 * if cut is set the reply was truncated, so the calls are answered
 * with errors instead.  every call gets a result, even after an
 * interrupt, as the APIs require.  sub is set for a sub-agent, whose
 * calls are shown indented and without their results.  returns 0 if
 * the user interrupted.
 */
int
runtools(struct conv *c, const struct tool *tl, int cut, int sub)
{
  const struct tool *t;
  struct msg *u, *a;
  struct block *b, *res;
  const char *in;
  size_t i, n;
  int r, snap;

  if (!(u = conv_msg(c, ROLE_USER))) nomem();
  a = &c->m[c->n - 2];
  for (i = 0; i < a->n; ++i) {
    b = &a->b[i];
    if (b->type != BLK_TOOL) continue;
    if (!(res = conv_block(u, BLK_RESULT))) nomem();
    if (!stralloc_copyb(&res->id, b->id.s, b->id.len)) nomem();
    in = b->text.len ? b->text.s : "{}";
    n = b->text.len ? b->text.len : 2;
    for (t = tl; t->name; ++t)
      if (str_len(t->name) == b->name.len &&
          !byte_diff(t->name, b->name.len, b->name.s))
        break;

    res->iserr = 1;
    if (intr) {
      stralloc_copys(&res->text, "not run: the user interrupted");
      continue;
    }
    announce(sub ? "  > " : "* ", b, in, n);
    if (cut)
      stralloc_copys(&res->text, "not run: the reply was cut off before "
          "this call was complete");
    else if (!t->name)
      stralloc_copys(&res->text, "there is no tool by that name");
    else if (!json_object(in, n))
      stralloc_copys(&res->text, "the input was not a valid JSON object");
    else if (!approve(t, in, n))
      stralloc_copys(&res->text, intr ? "not run: the user interrupted" :
          ruledeny.len ? ruledeny.s :
          "the user declined this call; ask them what to do instead");
    else if (!str_diff(t->name, "read_file") && tool_arg(in, n, "path", &tmp) &&
        stralloc_copys(&mwhy, "") &&
        (r = attach(u, &b->id, tmp.s, &mwhy)) != 0) {
      /* an image (or PDF): the model sees the thing itself */
      res = &u->b[u->n - 1 - (r > 0)];  /* attach may have moved it */
      if (!stralloc_copy(&res->text, &mwhy)) nomem();
      res->iserr = r < 0;
    } else {
      /* what the file was before, for /undo */
      snap = undodir.len && (!str_diff(t->name, "write_file") ||
          !str_diff(t->name, "edit_file") || !str_diff(t->name, "multi_edit")) &&
          tool_arg(in, n, "path", &tmp) &&
          undo_save(undodir.s, undoseq + 1, taskno, tmp.s);
      if (snap) ++undoseq;
      res->iserr = !t->run(in, n, &res->text);
      if (snap && res->iserr) {
        /* nothing changed after all */
        if (!stralloc_copy(&tmp, &undodir) || !stralloc_cats(&tmp, "/") ||
            !stralloc_catulong(&tmp, undoseq--) || !stralloc_0(&tmp))
          nomem();
        unlink(tmp.s);
      }
      if (intr) {
        stralloc_cats(&res->text, "\n[interrupted by the user]");
        res->iserr = 1;
      }
    }
    if (c == &conv) pending += res->text.len + 100;  /* and its framing */
    if (!sub)
      lines(res->iserr ? "  ! " : "  | ", res->text.s, res->text.len,
          !res->iserr && !str_diff(t->name ? t->name : "", "todo") ? 100 :
          !res->iserr && !str_diff(t->name ? t->name : "", "task") ? 8 : 4);
    else if (res->iserr)
      lines("    ! ", res->text.s, res->text.len, 1);
    flush();
  }
  if (intr && !sub) kwarn("interrupted", 0, 0);
  return !intr;
}
