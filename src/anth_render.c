#include "anth.h"
#include "api.h"
#include "conv.h"
#include "json.h"
#include "str.h"
#include "stralloc.h"

#define C(x) do { if (!(x)) return 0; } while (0)

static int
shown(const struct block *b)
{
  if (b->type == BLK_TEXT) return b->text.len > 0;
  if (b->type == BLK_THINK) return b->text.len || b->aux.len;
  return 1;
}

static int
block(stralloc *o, const struct block *b)
{
  switch (b->type) {
  case BLK_TEXT:
    C(stralloc_cats(o, "{\"type\":\"text\",\"text\":"));
    C(json_str(o, b->text.s, b->text.len));
    break;
  case BLK_THINK:
    C(stralloc_cats(o, "{\"type\":\"thinking\",\"thinking\":"));
    C(json_str(o, b->text.s, b->text.len));
    if (b->aux.len) {
      C(stralloc_cats(o, ",\"signature\":"));
      C(json_str(o, b->aux.s, b->aux.len));
    }
    break;
  case BLK_TOOL:
    C(stralloc_cats(o, "{\"type\":\"tool_use\",\"id\":"));
    C(json_str(o, b->id.s, b->id.len));
    C(stralloc_cats(o, ",\"name\":"));
    C(json_str(o, b->name.s, b->name.len));
    C(stralloc_cats(o, ",\"input\":"));
    if (json_object(b->text.s, b->text.len))
      C(stralloc_catb(o, b->text.s, b->text.len));
    else
      C(stralloc_cats(o, "{}"));
    break;
  case BLK_RESULT:
    C(stralloc_cats(o, "{\"type\":\"tool_result\",\"tool_use_id\":"));
    C(json_str(o, b->id.s, b->id.len));
    C(stralloc_cats(o, ",\"content\":"));
    C(json_str(o, b->text.s, b->text.len));
    if (b->iserr) C(stralloc_cats(o, ",\"is_error\":true"));
    break;
  case BLK_RAW:
    return stralloc_catb(o, b->text.s, b->text.len);
  }
  return stralloc_cats(o, "}");
}

/*
 * builds a streaming Anthropic Messages request.  for prompt caching
 * the system prompt carries a breakpoint, which covers the tools
 * too, and the top-level cache_control lets the server keep moving a
 * second breakpoint to the end of the growing conversation.  both
 * rely on everything before them being byte-identical from one
 * request to the next: the conversation is append-only and the tools
 * and system prompt never change during a session.
 */
int
anth_render(stralloc *o, const struct req *q)
{
  const struct tool *t;
  const struct msg *m;
  size_t i, j;
  int first = 1, firstb;

  C(stralloc_copys(o, "{\"model\":"));
  C(json_str(o, q->model, str_len(q->model)));
  C(stralloc_cats(o, ",\"max_tokens\":"));
  C(stralloc_catulong(o, q->max));
  C(stralloc_cats(o, ",\"stream\":true,\"cache_control\":{\"type\":\"ephemeral\"}"));
  if (q->system) {
    C(stralloc_cats(o, ",\"system\":[{\"type\":\"text\",\"text\":"));
    C(json_str(o, q->system, str_len(q->system)));
    C(stralloc_cats(o, ",\"cache_control\":{\"type\":\"ephemeral\"}}]"));
  }
  if (q->tools && q->tools->name) {
    C(stralloc_cats(o, ",\"tools\":["));
    for (t = q->tools; t->name; ++t) {
      if (t != q->tools) C(stralloc_cats(o, ","));
      C(stralloc_cats(o, "{\"name\":"));
      C(json_str(o, t->name, str_len(t->name)));
      C(stralloc_cats(o, ",\"description\":"));
      C(json_str(o, t->desc, str_len(t->desc)));
      C(stralloc_cats(o, ",\"input_schema\":"));
      C(stralloc_cats(o, t->schema));
      C(stralloc_cats(o, "}"));
    }
    C(stralloc_cats(o, "]"));
  }
  C(stralloc_cats(o, ",\"messages\":["));
  for (i = 0; i < q->conv->n; ++i) {
    m = &q->conv->m[i];
    for (j = 0; j < m->n && !shown(&m->b[j]); ++j) ;
    if (j == m->n) continue;
    if (!first) C(stralloc_cats(o, ","));
    first = 0;
    C(stralloc_cats(o, m->role == ROLE_USER ? "{\"role\":\"user\"" :
        "{\"role\":\"assistant\""));
    C(stralloc_cats(o, ",\"content\":["));
    firstb = 1;
    for (j = 0; j < m->n; ++j) {
      if (!shown(&m->b[j])) continue;
      if (!firstb) C(stralloc_cats(o, ","));
      firstb = 0;
      C(block(o, &m->b[j]));
    }
    C(stralloc_cats(o, "]}"));
  }
  return stralloc_cats(o, "]}");
}
