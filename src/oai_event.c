#include "api.h"
#include "byte.h"
#include "conv.h"
#include "json.h"
#include "oai.h"
#include "scan.h"
#include "stralloc.h"

/* appends the string at path to out: 1 if appended, 0 if absent, -1 bad */
static int
str(const char *d, size_t n, const char *path, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(d, n, path, &v, &vn) || *v != '"') return 0;
  return json_unstr(out, v, vn) ? 1 : -1;
}

static int
num(const char *d, size_t n, const char *path, unsigned long *u)
{
  const char *v;
  size_t vn;

  return json_get(d, n, path, &v, &vn) && scan_ulongb(v, vn, u) == vn;
}

static struct block *
tool(struct msg *m, long idx)
{
  struct block *b;
  size_t i;

  for (i = 0; i < m->n; ++i)
    if (m->b[i].type == BLK_TOOL && m->b[i].idx == idx) return &m->b[i];
  if (!(b = conv_block(m, BLK_TOOL))) return 0;
  b->idx = idx;
  return b;
}

/* one entry of delta.tool_calls; pieces of it arrive across chunks */
static int
call(struct msg *m, const char *d, size_t n, long k, stralloc *tmp)
{
  struct block *b;
  const char *v;
  size_t vn;
  unsigned long u;
  long idx = k;

  if (json_get(d, n, "index", &v, &vn) && scan_ulongb(v, vn, &u) == vn)
    idx = (long)u;
  if (!(b = tool(m, idx))) return -1;
  if (!stralloc_copys(tmp, "")) return -1;
  if (str(d, n, "id", tmp) < 0) return -1;
  if (tmp->len && !stralloc_copyb(&b->id, tmp->s, tmp->len)) return -1;
  if (!stralloc_copys(tmp, "")) return -1;
  if (str(d, n, "function.name", tmp) < 0) return -1;
  if (tmp->len && !stralloc_copyb(&b->name, tmp->s, tmp->len)) return -1;
  return str(d, n, "function.arguments", &b->text) < 0 ? -1 : 1;
}

static int
stop(const char *v, size_t n)
{
  if (json_is(v, n, "stop")) return STOP_END;
  if (json_is(v, n, "tool_calls") || json_is(v, n, "function_call"))
    return STOP_TOOL;
  if (json_is(v, n, "length")) return STOP_MAX;
  if (json_is(v, n, "content_filter")) return STOP_REFUSAL;
  return STOP_OTHER;
}

/*
 * handles the data of one Chat Completions stream chunk, building
 * the assistant message m.  reasoning (reasoning_content, as sent by
 * LM Studio, llama.cpp and DeepSeek, or reasoning) is displayed but
 * not kept.  returns API_MORE, API_DONE at [DONE], or API_ERROR with
 * the message in r->err.
 */
int
oai_event(struct msg *m, const char *d, size_t n, struct reply *r)
{
  static stralloc path, tmp;
  struct block *b;
  const char *v;
  size_t vn, k0;
  long k;

  if (n == 6 && !byte_diff(d, 6, "[DONE]")) return API_DONE;
  if (json_get(d, n, "error", &v, &vn)) {
    if (str(d, n, "error.message", &r->err) <= 0)
      stralloc_cats(&r->err, "the server reported an error");
    if ((json_get(d, n, "error.type", &v, &vn) &&
        (json_is(v, vn, "server_error") || json_is(v, vn, "rate_limit_error")))
        || (json_get(d, n, "error.code", &v, &vn) &&
        json_is(v, vn, "rate_limit_exceeded")))
      r->retry = 1;
    return API_ERROR;
  }
  if (json_get(d, n, "usage", &v, &vn) && *v == '{') {
    /* prompt_tokens includes any cached tokens; keep them apart */
    num(v, vn, "prompt_tokens", &r->u.in);
    num(v, vn, "completion_tokens", &r->u.out);
    if (num(v, vn, "prompt_tokens_details.cached_tokens", &r->u.cread) &&
        r->u.cread <= r->u.in)
      r->u.in -= r->u.cread;
  }
  if (!json_get(d, n, "choices.0", &v, &vn)) return API_MORE;

  k0 = r->text.len;
  if (str(d, n, "choices.0.delta.content", &r->text) < 0) goto bad;
  if (r->text.len > k0) {
    b = m->n ? &m->b[m->n - 1] : 0;
    if (!b || b->type != BLK_TEXT)
      if (!(b = conv_block(m, BLK_TEXT))) goto nomem;
    if (!stralloc_catb(&b->text, r->text.s + k0, r->text.len - k0))
      goto nomem;
  }
  if (str(d, n, "choices.0.delta.reasoning_content", &r->think) < 0) goto bad;
  if (str(d, n, "choices.0.delta.reasoning", &r->think) < 0) goto bad;

  for (k = 0;; ++k) {
    if (!stralloc_copys(&path, "choices.0.delta.tool_calls.")) goto nomem;
    if (!stralloc_catulong(&path, (unsigned long)k)) goto nomem;
    if (!stralloc_0(&path)) goto nomem;
    if (!json_get(d, n, path.s, &v, &vn)) break;
    if (call(m, v, vn, k, &tmp) < 0) goto bad;
  }

  if (json_get(d, n, "choices.0.finish_reason", &v, &vn) && *v == '"')
    r->stop = stop(v, vn);
  return API_MORE;

nomem:
  stralloc_copys(&r->err, "out of memory");
  return API_ERROR;
bad:
  stralloc_copys(&r->err, "malformed stream chunk");
  return API_ERROR;
}
