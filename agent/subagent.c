#include "agent.h"

static int
subloop(struct conv *c)
{
  struct msg *m;
  int step, r;

  for (step = 0; step < SUBSTEPS; ++step) {
    r = exchange(c, subsys.s, subtools, model, max, 0);
    if (r <= 0) return r;
    m = &c->m[c->n - 1];
    if (reply.stop == STOP_PAUSE && !hastools(m)) continue;
    if (!hastools(m)) return 1;
    if (!runtools(c, subtools, reply.stop == STOP_MAX, 1)) return -1;
    if (reply.stop == STOP_MAX) return 1;
  }
  return 1;  /* out of steps: report what there is */
}

/* task: description, prompt */
int
subagent(const char *in, size_t n, stralloc *out)
{
  static int depth;
  struct conv sc = { 0, 0, 0 };
  struct msg *m;
  struct block *b;
  size_t i;
  int r, ok = 0;

  if (depth) {
    stralloc_copys(out, "a sub-agent cannot start sub-agents");
    return 0;
  }
  if (!(m = conv_msg(&sc, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
    nomem();
  if (!tool_arg(in, n, "prompt", &b->text) || !b->text.len) {
    stralloc_copys(out, "prompt is required");
    goto out;
  }
  ++depth;
  r = subloop(&sc);
  --depth;
  if (r < 0) { stralloc_copys(out, "the sub-agent was interrupted"); goto out; }
  if (!r) { stralloc_copys(out, "the sub-agent failed; see above"); goto out; }

  /* its report: the text of its last reply */
  stralloc_copys(out, "");
  m = &sc.m[sc.n - 1];
  for (i = 0; m->role == ROLE_ASSISTANT && i < m->n; ++i)
    if (m->b[i].type == BLK_TEXT) {
      if (out->len) stralloc_cats(out, "\n\n");
      stralloc_cat(out, &m->b[i].text);
    }
  if (!out->len) stralloc_copys(out, "the sub-agent finished without a report");
  ok = 1;
out:
  while (sc.n) conv_drop(&sc);
  alloc_free(sc.m);
  return ok;
}
