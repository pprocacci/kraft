#include "conv.h"
#include "json.h"
#include "stralloc.h"

#define C(x) do { if (!(x)) return 0; } while (0)

static int
field(stralloc *o, const char *name, const stralloc *v)
{
  if (!v->len) return 1;
  C(stralloc_cats(o, ",\""));
  C(stralloc_cats(o, name));
  C(stralloc_cats(o, "\":"));
  return json_str(o, v->s, v->len);
}

/*
 * appends the conversation to o as text, one line per message:
 *   {"r":0,"b":[{"t":2,"e":1,"id":"...","name":"...","text":"...",
 *     "aux":"..."},...]}
 * r is the role and t the block type, as in conv.h; e marks an error
 * result.  empty fields are left out.  conv_load reads it back.
 */
int
conv_save(stralloc *o, const struct conv *c)
{
  const struct msg *m;
  const struct block *b;
  size_t i, j;

  for (i = 0; i < c->n; ++i) {
    m = &c->m[i];
    C(stralloc_cats(o, "{\"r\":"));
    C(stralloc_catulong(o, (unsigned long)m->role));
    C(stralloc_cats(o, ",\"b\":["));
    for (j = 0; j < m->n; ++j) {
      b = &m->b[j];
      if (j) C(stralloc_cats(o, ","));
      C(stralloc_cats(o, "{\"t\":"));
      C(stralloc_catulong(o, (unsigned long)b->type));
      if (b->iserr) C(stralloc_cats(o, ",\"e\":1"));
      C(field(o, "id", &b->id));
      C(field(o, "name", &b->name));
      C(field(o, "text", &b->text));
      C(field(o, "aux", &b->aux));
      C(stralloc_cats(o, "}"));
    }
    C(stralloc_cats(o, "]}\n"));
  }
  return 1;
}
