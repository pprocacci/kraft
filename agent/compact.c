#include "agent.h"

/*
 * replaces the conversation with a summary the model writes of it.
 * the request has the same system prompt and tools as the rest, so
 * most of it is read from the cache.  if carry is set a task is under
 * way, and the model is told to carry on with it.  returns 1 if the
 * conversation was compacted.
 */
int
compact(int carry)
{
  struct msg *m;
  struct block *b;
  stralloc sum = STRALLOC_ZERO;
  size_t i;
  int r;

  nl();
  outs("(compacting the conversation to save space...)\n");
  flush();
  if (!(m = conv_msg(&conv, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
    nomem();
  if (!stralloc_copys(&b->text, "[Stop here, and instead of carrying on, "
      "write a summary of this whole conversation that will replace it. "
      "Include: the user's requests, quoting the latest one exactly; what "
      "has been done so far; the files read or changed, and the facts "
      "about them that matter; errors met and how they were dealt with; "
      "and exactly what remains to be done. Be complete but concise: the "
      "summary is all that will be kept. Do not call any tools.]"))
    nomem();
  overflow = 0;
  r = exchange(&conv, sys.s, tools, model, max, 0);
  if (r > 0) {
    m = &conv.m[conv.n - 1];
    for (i = 0; i < m->n; ++i)
      if (m->b[i].type == BLK_TEXT &&
          !stralloc_catb(&sum, m->b[i].text.s, m->b[i].text.len))
        nomem();
  }
  if (r <= 0 || !sum.len) {
    /* take the request back out, so the conversation is as it was */
    if (r > 0) conv_drop(&conv);
    m = &conv.m[conv.n - 1];
    if (m->role == ROLE_USER && m->n && m->b[m->n - 1].type == BLK_TEXT) {
      stralloc_free(&m->b[m->n - 1].text);
      if (!--m->n) conv_drop(&conv);
    }
    kwarn(overflow ? "the conversation is too long even to summarize; "
        "/clear starts afresh" : "unable to compact the conversation", 0, 0);
    overflow = 0;
    stralloc_free(&sum);
    return 0;
  }

  while (conv.n) conv_drop(&conv);
  if (!(m = conv_msg(&conv, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
    nomem();
  if (!stralloc_copys(&b->text, "[The conversation so far was replaced by "
      "this summary to save space.]\n\n") ||
      !stralloc_catb(&b->text, sum.s, sum.len) ||
      !stralloc_cats(&b->text, carry ? "\n\n[End of summary. Carry on "
      "with the task from where it was left.]" : "\n\n[End of summary.]"))
    nomem();
  lastmain = nousage;
  pending = b->text.len;
  stralloc_free(&sum);
  if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, estimate()) ||
      !stralloc_0(&tmp))
    nomem();
  outs("(compacted to about ");
  outs(tmp.s);
  outs(" tokens)\n");
  flush();
  return 1;
}
