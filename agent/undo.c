#include "agent.h"

/* puts back the files the last task with changes wrote; 1 if any */
int
undo(void)
{
  stralloc p = STRALLOC_ZERO, d = STRALLOC_ZERO, names = STRALLOC_ZERO;
  stralloc fn = STRALLOC_ZERO;
  struct msg *m;
  struct block *b;
  unsigned long t, task = 0;
  int ex, n = 0;

  for (; undoseq && undo_load(undodir.s, undoseq, &t, &p, &d, &ex); --undoseq) {
    if (!task) task = t;
    if (t != task) break;
    if (ex ? !file_put(p.s, d.s, d.len) : (unlink(p.s) == -1 && errno != ENOENT))
      kwarnsys("unable to put back ", p.s);
    else {
      sgr(DIM);
      outs(ex ? "  restored " : "  removed ");
      outs(p.s);
      sgr(0);
      outs("\n");
      if (n++ && !stralloc_cats(&names, ", ")) nomem();
      if (!stralloc_cat(&names, &p)) nomem();
    }
    if (!stralloc_copy(&fn, &undodir) || !stralloc_cats(&fn, "/") ||
        !stralloc_catulong(&fn, undoseq) || !stralloc_0(&fn))
      nomem();
    unlink(fn.s);
  }
  if (n) {
    /* the model's picture of those files is out of date now */
    if (!(m = conv_msg(&conv, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
      nomem();
    if (!stralloc_copys(&b->text, "[The user undid the file changes made in "
        "an earlier task. These are back as they were before it: ") ||
        !stralloc_cat(&b->text, &names) ||
        !stralloc_cats(&b->text, ". Read them again before relying on "
        "their contents.]"))
      nomem();
    pending += b->text.len;
  }
  stralloc_free(&p);
  stralloc_free(&d);
  stralloc_free(&names);
  stralloc_free(&fn);
  return n > 0;
}
