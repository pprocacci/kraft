#include "anth.h"
#include "api.h"
#include "conv.h"
#include "json.h"
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

/* the same, also copied into a second stralloc */
static int
str2(const char *d, size_t n, const char *path, stralloc *a, stralloc *b)
{
  size_t k = a->len;
  int r;

  if ((r = str(d, n, path, a)) <= 0) return r;
  return stralloc_catb(b, a->s + k, a->len - k) ? 1 : -1;
}

static int
num(const char *d, size_t n, const char *path, unsigned long *u)
{
  const char *v;
  size_t vn;

  return json_get(d, n, path, &v, &vn) && scan_ulong(v, u) == vn;
}

/* takes whichever usage counts the object at path has */
static void
usage(const char *d, size_t n, const char *path, struct usage *u)
{
  const char *v;
  size_t vn;

  if (!json_get(d, n, path, &v, &vn)) return;
  num(v, vn, "input_tokens", &u->in);
  num(v, vn, "cache_read_input_tokens", &u->cread);
  num(v, vn, "cache_creation_input_tokens", &u->cwrite);
  num(v, vn, "output_tokens", &u->out);
}

static struct block *
find(struct msg *m, unsigned long idx)
{
  size_t i;

  for (i = 0; i < m->n; ++i)
    if (m->b[i].idx == (long)idx) return &m->b[i];
  return 0;
}

static int
stop(const char *v, size_t n)
{
  if (json_is(v, n, "end_turn") || json_is(v, n, "stop_sequence"))
    return STOP_END;
  if (json_is(v, n, "tool_use")) return STOP_TOOL;
  if (json_is(v, n, "max_tokens")) return STOP_MAX;
  if (json_is(v, n, "refusal")) return STOP_REFUSAL;
  if (json_is(v, n, "pause_turn")) return STOP_PAUSE;
  return STOP_OTHER;
}

/*
 * handles the JSON data of one Anthropic stream event, building the
 * assistant message m.  reply text and reasoning are also appended
 * to r for display.  returns API_MORE, API_DONE at message_stop, or
 * API_ERROR with the message in r->err.
 */
int
anth_event(struct msg *m, const char *d, size_t n, struct reply *r)
{
  const char *v, *cb;
  size_t vn, cn;
  unsigned long idx;
  struct block *b;
  int k = 0;

  if (!json_get(d, n, "type", &v, &vn)) goto bad;

  if (json_is(v, vn, "content_block_start")) {
    if (!num(d, n, "index", &idx)) goto bad;
    if (!json_get(d, n, "content_block", &cb, &cn)) goto bad;
    if (!json_get(cb, cn, "type", &v, &vn)) goto bad;
    if (json_is(v, vn, "text")) {
      if (!(b = conv_block(m, BLK_TEXT))) goto nomem;
      k = str2(cb, cn, "text", &b->text, &r->text);
    } else if (json_is(v, vn, "thinking")) {
      if (!(b = conv_block(m, BLK_THINK))) goto nomem;
      k = str2(cb, cn, "thinking", &b->text, &r->think);
      if (k >= 0) k = str(cb, cn, "signature", &b->aux);
    } else if (json_is(v, vn, "tool_use") ||
        json_is(v, vn, "server_tool_use")) {
      if (!(b = conv_block(m, json_is(v, vn, "tool_use") ? BLK_TOOL :
          BLK_SERVER)))
        goto nomem;
      k = str(cb, cn, "id", &b->id);
      if (k >= 0) k = str(cb, cn, "name", &b->name);
      /* the input usually streams in; take it here if it came whole */
      if (k >= 0 && json_get(cb, cn, "input", &v, &vn) && json_object(v, vn) &&
          !(vn == 2 && v[1] == '}') && !stralloc_copyb(&b->text, v, vn))
        goto nomem;
    } else {
      /* redacted_thinking, server tool results: keep them verbatim */
      if (!(b = conv_block(m, BLK_RAW))) goto nomem;
      if (!stralloc_copyb(&b->text, cb, cn)) goto nomem;
    }
    if (k < 0) goto bad;
    b->idx = (long)idx;
    return API_MORE;
  }

  if (json_is(v, vn, "content_block_delta")) {
    if (!num(d, n, "index", &idx) || !(b = find(m, idx))) goto bad;
    if (b->type == BLK_RAW) return API_MORE;  /* complete as it came */
    if (!json_get(d, n, "delta.type", &v, &vn)) goto bad;
    if (json_is(v, vn, "text_delta"))
      k = str2(d, n, "delta.text", &b->text, &r->text);
    else if (json_is(v, vn, "thinking_delta"))
      k = str2(d, n, "delta.thinking", &b->text, &r->think);
    else if (json_is(v, vn, "signature_delta"))
      k = str(d, n, "delta.signature", &b->aux);
    else if (json_is(v, vn, "input_json_delta"))
      k = str(d, n, "delta.partial_json", &b->text);
    if (k < 0) goto bad;
    return API_MORE;
  }

  if (json_is(v, vn, "content_block_stop")) {
    /* show the user what a server tool is doing, such as a web search */
    if (num(d, n, "index", &idx) && (b = find(m, idx)) &&
        b->type == BLK_SERVER) {
      stralloc_cats(&r->text, "\n* ");
      stralloc_cat(&r->text, &b->name);
      stralloc_cats(&r->text, "(");
      if (json_get(b->text.s, b->text.len, "query", &v, &vn) && *v == '"')
        json_unstr(&r->text, v, vn);
      stralloc_cats(&r->text, ")\n");
    }
    return API_MORE;
  }

  if (json_is(v, vn, "message_delta")) {
    if (json_get(d, n, "delta.stop_reason", &v, &vn) && *v == '"')
      r->stop = stop(v, vn);
    usage(d, n, "usage", &r->u);
    return API_MORE;
  }
  if (json_is(v, vn, "message_start")) {
    usage(d, n, "message.usage", &r->u);
    return API_MORE;
  }
  if (json_is(v, vn, "message_stop")) return API_DONE;
  if (json_is(v, vn, "error")) {
    if (str(d, n, "error.message", &r->err) <= 0)
      stralloc_cats(&r->err, "the server reported an error");
    return API_ERROR;
  }
  return API_MORE;  /* message_start was above; ping, ... */

nomem:
  stralloc_copys(&r->err, "out of memory");
  return API_ERROR;
bad:
  stralloc_copys(&r->err, "malformed stream event");
  return API_ERROR;
}
