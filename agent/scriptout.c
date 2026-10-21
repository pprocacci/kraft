#include "agent.h"

static void
field(stralloc *o, const char *name, unsigned long u)
{
  if (!stralloc_cats(o, ",\"") || !stralloc_cats(o, name) ||
      !stralloc_cats(o, "\":") || !stralloc_catulong(o, u))
    nomem();
}

/*
 * for -p and -j: the outcome of the task, on standard output (the
 * rest went to standard error).  -p writes the final reply's text;
 * -j a JSON object with that and the session's figures.
 */
void
scriptout(int json)
{
  stralloc t = STRALLOC_ZERO, o = STRALLOC_ZERO, c = STRALLOC_ZERO;
  const struct msg *m;
  size_t i, j;

  /* the text of the last reply */
  for (i = conv.n; i-- > 0;) {
    m = &conv.m[i];
    if (m->role != ROLE_ASSISTANT) continue;
    for (j = 0; j < m->n; ++j)
      if (m->b[j].type == BLK_TEXT && m->b[j].text.len) {
        if (t.len && !stralloc_cats(&t, "\n\n")) nomem();
        if (!stralloc_cat(&t, &m->b[j].text)) nomem();
      }
    if (t.len) break;
  }
  buffer_flush(buffer_1);
  buffer_1->fd = 1;
  if (!json) {
    if (t.len && t.s[t.len - 1] != '\n' && !stralloc_cats(&t, "\n")) nomem();
    buffer_put(buffer_1, t.s, t.len);
    buffer_flush(buffer_1);
    goto out;
  }
  if (!stralloc_copys(&o, "{\"result\":") || !json_str(&o, t.s, t.len) ||
      !stralloc_cats(&o, status ? ",\"error\":true" : ",\"error\":false"))
    nomem();
  field(&o, "status", (unsigned long)status);
  if (!stralloc_cats(&o, ",\"model\":") || !json_str(&o, model, str_len(model)))
    nomem();
  field(&o, "requests", requests);
  field(&o, "input_tokens", total.in);
  field(&o, "cache_read_tokens", total.cread);
  field(&o, "cache_write_tokens", total.cwrite);
  field(&o, "output_tokens", total.out);
  catcost(&c);
  if (!stralloc_cats(&o, ",\"cost_usd\":")) nomem();
  if (c.len && c.s[c.len - 1] != '+') {
    /* "$0.0187" as a number */
    if (!stralloc_catb(&o, c.s + 1, c.len - 1)) nomem();
  } else if (!stralloc_cats(&o, "null")) nomem();
  if (!stralloc_cats(&o, ",\"session\":")) nomem();
  if (sessfn.len) {
    if (!json_str(&o, sessfn.s, str_len(sessfn.s))) nomem();
  } else if (!stralloc_cats(&o, "null")) nomem();
  if (!stralloc_cats(&o, "}\n")) nomem();
  buffer_put(buffer_1, o.s, o.len);
  buffer_flush(buffer_1);
out:
  stralloc_free(&t);
  stralloc_free(&o);
  stralloc_free(&c);
}
