#include "agent.h"

#define MAXCALLS 64  /* in one reply, for running side by side */

/* tool name in list tl, or the end of the list (a null name) */
static const struct tool *
find(const struct tool *tl, const stralloc *name)
{
  for (; tl->name; ++tl)
    if (str_len(tl->name) == name->len &&
        !byte_diff(tl->name, name->len, name->s))
      break;
  return tl;
}

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
  static stralloc hookout;
  /* calls run side by side, ahead of their turn */
  stralloc pout[MAXCALLS], tout[MAXCALLS];  /* not static: a sub-agent comes back here */
  int pok[MAXCALLS], tok[MAXCALLS], pre[MAXCALLS];
  const struct tool *pt[MAXCALLS];
  const char *pin[MAXCALLS];
  size_t pn[MAXCALLS], pi[MAXCALLS], k, j;

  if (!(u = conv_msg(c, ROLE_USER))) nomem();
  a = &c->m[c->n - 2];
  for (i = 0; i < MAXCALLS; ++i) {
    pre[i] = 0;
    pout[i].s = tout[i].s = 0;
    pout[i].len = tout[i].len = pout[i].a = tout[i].a = 0;
  }
  for (i = 0; i < a->n; ++i) {
    b = &a->b[i];
    if (b->type != BLK_TOOL) continue;
    if (!(res = conv_block(u, BLK_RESULT))) nomem();
    if (!stralloc_copyb(&res->id, b->id.s, b->id.len)) nomem();
    in = b->text.len ? b->text.s : "{}";
    n = b->text.len ? b->text.len : 2;
    t = find(tl, &b->name);

    res->iserr = 1;
    if (intr) {
      stralloc_copys(&res->text, "not run: the user interrupted");
      continue;
    }
    /* this and the calls after it that change nothing: all at once */
    if (!cut && i < MAXCALLS && !pre[i]) {
      for (k = 0, j = i; j < a->n && j < MAXCALLS && k < MAXCALLS; ++j) {
        if (a->b[j].type != BLK_TOOL) continue;
        pt[k] = find(tl, &a->b[j].name);
        pin[k] = a->b[j].text.len ? a->b[j].text.s : "{}";
        pn[k] = a->b[j].text.len ? a->b[j].text.len : 2;
        if (!canpar(pt[k], pin[k], pn[k])) break;
        pi[k++] = j;
      }
      if (k > 1) {
        for (j = 0; j < k; ++j) {
          announce(sub ? "  > " : "* ", &a->b[pi[j]], pin[j], pn[j]);
          pre[pi[j]] = 1;
        }
        parallel(pt, pin, pn, k, tout, tok);
        for (j = 0; j < k; ++j) {
          if (!stralloc_copy(&pout[pi[j]], &tout[j])) nomem();
          pok[pi[j]] = tok[j];
        }
      }
    }
    if (!pre[i]) announce(sub ? "  > " : "* ", b, in, n);
    /* an edit's diff, whether it will be asked about or not */
    if (!cut && t->name && isedit(t->name) && json_object(in, n))
      showedit(t, in, n);
    if (cut)
      stralloc_copys(&res->text, "not run: the reply was cut off before "
          "this call was complete");
    else if (!t->name)
      stralloc_copys(&res->text, "there is no tool by that name");
    else if (!json_object(in, n))
      stralloc_copys(&res->text, "the input was not a valid JSON object");
    else if (pre[i]) {
      /* it has run already, alongside others */
      if (!stralloc_copy(&res->text, &pout[i])) nomem();
      res->iserr = !pok[i];
      if (nhooks) {
        hook(HOOK_AFTER, t, in, n, &res->text, res->iserr, &hookout);
        if (hookout.len && (!stralloc_cats(&res->text, "\n\n[from the user's "
            "hooks]\n") || !stralloc_cat(&res->text, &hookout)))
          nomem();
      }
    } else if (inchild && !str_diff(t->name, "bash") &&
        tool_argflag(in, n, "background"))
      stralloc_copys(&res->text, "a sub-agent running alongside others "
          "cannot start background commands; run it in the foreground");
    else if (!approve(t, in, n))
      stralloc_copys(&res->text, intr ? "not run: the user interrupted" :
          ruledeny.len ? ruledeny.s :
          "the user declined this call; ask them what to do instead");
    else if (nhooks && !hook(HOOK_BEFORE, t, in, n, 0, 0, &hookout)) {
      if (!stralloc_copys(&res->text, "blocked by the user's hook:\n") ||
          !stralloc_cat(&res->text, &hookout))
        nomem();
    } else if (!str_diff(t->name, "read_file") && tool_arg(in, n, "path", &tmp) &&
        stralloc_copys(&mwhy, "") &&
        (r = attach(u, &b->id, tmp.s, &mwhy)) != 0) {
      /* an image (or PDF): the model sees the thing itself */
      res = &u->b[u->n - 1 - (r > 0)];  /* attach may have moved it */
      if (!stralloc_copy(&res->text, &mwhy)) nomem();
      res->iserr = r < 0;
    } else {
      /* what the file was before, for /undo */
      snap = 0;
      if (undodir.len && isedit(t->name) && tool_arg(in, n, "path", &tmp)) {
        /* alongside others, the next free number is the one to use */
        plock(1);
        while (inchild) {
          if (!stralloc_copy(&mwhy, &undodir) || !stralloc_cats(&mwhy, "/") ||
              !stralloc_catulong(&mwhy, undoseq + 1) || !stralloc_0(&mwhy))
            nomem();
          if (access(mwhy.s, F_OK) == -1) break;
          ++undoseq;
        }
        snap = undo_save(undodir.s, undoseq + 1, taskno, tmp.s);
        if (snap) ++undoseq;
        plock(0);
      }
      res->iserr = !(t->mcp ? mcpcall(t, in, n, &res->text) :
          t->run(in, n, &res->text));
      if (snap && res->iserr && !inchild) {
        /* nothing changed after all */
        if (!stralloc_copy(&tmp, &undodir) || !stralloc_cats(&tmp, "/") ||
            !stralloc_catulong(&tmp, undoseq--) || !stralloc_0(&tmp))
          nomem();
        unlink(tmp.s);
      }
      if (intr) {
        stralloc_cats(&res->text, "\n[interrupted by the user]");
        res->iserr = 1;
      } else if (nhooks) {
        /* a formatter's or a linter's say goes to the model too */
        hook(HOOK_AFTER, t, in, n, &res->text, res->iserr, &hookout);
        if (hookout.len && (!stralloc_cats(&res->text, "\n\n[from the user's "
            "hooks]\n") || !stralloc_cat(&res->text, &hookout)))
          nomem();
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
  for (i = 0; i < MAXCALLS; ++i) {
    stralloc_free(&pout[i]);
    stralloc_free(&tout[i]);
  }
  return !intr;
}
