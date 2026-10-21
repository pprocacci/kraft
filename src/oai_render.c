#include "api.h"
#include "byte.h"
#include "conv.h"
#include "json.h"
#include "oai.h"
#include "str.h"
#include "stralloc.h"

#define C(x) do { if (!(x)) goto bad; } while (0)

static int
sep(stralloc *o, int *first)
{
  if (*first) { *first = 0; return 1; }
  return stralloc_cats(o, ",");
}

/*
 * builds a streaming Chat Completions request.  tool results become
 * "tool" messages; reasoning is not sent back.
 */
int
oai_render(stralloc *o, const struct req *q)
{
  stralloc text = STRALLOC_ZERO;
  const struct tool *t;
  const struct msg *m;
  const struct block *b;
  size_t i, j;
  int first = 1, ntools, nmedia, firstt;

  C(stralloc_copys(o, "{\"model\":"));
  C(json_str(o, q->model, str_len(q->model)));
  C(stralloc_cats(o, ",\"max_tokens\":"));
  C(stralloc_catulong(o, q->max));
  C(stralloc_cats(o, ",\"stream\":true,"
      "\"stream_options\":{\"include_usage\":true}"));
  if (q->tools && q->tools->name) {
    C(stralloc_cats(o, ",\"tools\":["));
    firstt = 1;
    for (t = q->tools; t->name; ++t) {
      if (t->raw) continue;  /* Anthropic server tools mean nothing here */
      C(sep(o, &firstt));
      C(stralloc_cats(o, "{\"type\":\"function\",\"function\":{\"name\":"));
      C(json_str(o, t->name, str_len(t->name)));
      C(stralloc_cats(o, ",\"description\":"));
      C(json_str(o, t->desc, str_len(t->desc)));
      C(stralloc_cats(o, ",\"parameters\":"));
      C(stralloc_cats(o, t->schema));
      C(stralloc_cats(o, "}}"));
    }
    C(stralloc_cats(o, "]"));
  }
  C(stralloc_cats(o, ",\"messages\":["));
  if (q->system) {
    C(sep(o, &first));
    C(stralloc_cats(o, "{\"role\":\"system\",\"content\":"));
    C(json_str(o, q->system, str_len(q->system)));
    C(stralloc_cats(o, "}"));
  }

  for (i = 0; i < q->conv->n; ++i) {
    m = &q->conv->m[i];
    C(stralloc_copys(&text, ""));
    ntools = nmedia = 0;
    for (j = 0; j < m->n; ++j) {
      b = &m->b[j];
      if (b->type == BLK_TEXT && b->text.len) {
        if (text.len) C(stralloc_cats(&text, "\n\n"));
        C(stralloc_catb(&text, b->text.s, b->text.len));
      } else if (b->type == BLK_MEDIA && b->text.len)
        ++nmedia;
      else if (b->type == BLK_TOOL)
        ++ntools;
      else if (b->type == BLK_RESULT) {
        C(sep(o, &first));
        C(stralloc_cats(o, "{\"role\":\"tool\",\"tool_call_id\":"));
        C(json_str(o, b->id.s, b->id.len));
        C(stralloc_cats(o, ",\"content\":"));
        C(json_str(o, b->text.s, b->text.len));
        C(stralloc_cats(o, "}"));
      }
    }
    if (m->role == ROLE_USER) {
      if (!text.len && !nmedia) continue;
      C(sep(o, &first));
      C(stralloc_cats(o, "{\"role\":\"user\",\"content\":"));
      if (!nmedia) {
        C(json_str(o, text.s, text.len));
        C(stralloc_cats(o, "}"));
        continue;
      }
      /*
       * tool messages carry only text here, so images from tools
       * come in this user message, after them
       */
      C(stralloc_cats(o, "[{\"type\":\"text\",\"text\":"));
      if (text.len) C(json_str(o, text.s, text.len));
      else C(json_str(o, "(the images from the tool calls above)", 38));
      C(stralloc_cats(o, "}"));
      for (j = 0; j < m->n; ++j) {
        b = &m->b[j];
        if (b->type != BLK_MEDIA || !b->text.len) continue;
        if (b->aux.len > 6 && !byte_diff(b->aux.s, 6, "image/")) {
          C(stralloc_cats(o, ",{\"type\":\"image_url\",\"image_url\":"
              "{\"url\":\"data:"));
          C(stralloc_catb(o, b->aux.s, b->aux.len));
          C(stralloc_cats(o, ";base64,"));
          C(stralloc_catb(o, b->text.s, b->text.len));
          C(stralloc_cats(o, "\"}}"));
        } else
          C(stralloc_cats(o, ",{\"type\":\"text\",\"text\":"
              "\"[a PDF was attached, but this API cannot carry PDFs]\"}"));
      }
      C(stralloc_cats(o, "]}"));
      continue;
    }
    if (!text.len && !ntools) continue;
    C(sep(o, &first));
    C(stralloc_cats(o, "{\"role\":\"assistant\",\"content\":"));
    if (text.len) C(json_str(o, text.s, text.len));
    else C(stralloc_cats(o, "null"));
    if (ntools) {
      C(stralloc_cats(o, ",\"tool_calls\":["));
      firstt = 1;
      for (j = 0; j < m->n; ++j) {
        b = &m->b[j];
        if (b->type != BLK_TOOL) continue;
        C(sep(o, &firstt));
        C(stralloc_cats(o, "{\"id\":"));
        C(json_str(o, b->id.s, b->id.len));
        C(stralloc_cats(o, ",\"type\":\"function\",\"function\":{\"name\":"));
        C(json_str(o, b->name.s, b->name.len));
        C(stralloc_cats(o, ",\"arguments\":"));
        if (json_object(b->text.s, b->text.len))
          C(json_str(o, b->text.s, b->text.len));
        else
          C(stralloc_cats(o, "\"{}\""));
        C(stralloc_cats(o, "}}"));
      }
      C(stralloc_cats(o, "]"));
    }
    C(stralloc_cats(o, "}"));
  }
  C(stralloc_cats(o, "]}"));
  stralloc_free(&text);
  return 1;
bad:
  stralloc_free(&text);
  return 0;
}
