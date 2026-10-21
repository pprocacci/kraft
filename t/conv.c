#include <stdio.h>
#include <string.h>
#include "anth.h"
#include "api.h"
#include "conv.h"
#include "oai.h"
#include "stralloc.h"
#include "tool.h"

static int fails;

static void
check(int ok, const char *what)
{
  if (!ok) { printf("FAIL %s\n", what); ++fails; }
}

static int
is(const stralloc *sa, const char *s)
{
  return sa->len == strlen(s) && !memcmp(sa->s, s, sa->len);
}

static int
feed(int (*ev)(struct msg *, const char *, size_t, struct reply *),
    struct msg *m, struct reply *r, const char *d)
{
  stralloc_copys(&r->text, "");
  stralloc_copys(&r->think, "");
  return ev(m, d, strlen(d), r);
}

static int
has(stralloc *o, const char *s)
{
  stralloc_0(o);
  --o->len;
  return strstr(o->s, s) != 0;
}

static void
anthropic(void)
{
  static const char *ev[] = {
    "{\"type\":\"message_start\",\"message\":{\"id\":\"m1\",\"usage\":"
        "{\"input_tokens\":12,\"cache_read_input_tokens\":3000,"
        "\"cache_creation_input_tokens\":40,\"output_tokens\":1}}}",
    "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":"
        "{\"type\":\"thinking\",\"thinking\":\"\"}}",
    "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":"
        "{\"type\":\"thinking_delta\",\"thinking\":\"hmm\"}}",
    "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":"
        "{\"type\":\"signature_delta\",\"signature\":\"sig\"}}",
    "{\"type\":\"content_block_start\",\"index\":1,\"content_block\":"
        "{\"type\":\"text\",\"text\":\"\"}}",
    "{\"type\":\"content_block_delta\",\"index\":1,\"delta\":"
        "{\"type\":\"text_delta\",\"text\":\"Hi\"}}",
    "{\"type\":\"content_block_start\",\"index\":2,\"content_block\":"
        "{\"type\":\"tool_use\",\"id\":\"tu_1\",\"name\":\"bash\",\"input\":{}}}",
    "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":"
        "{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"comm\"}}",
    "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":"
        "{\"type\":\"input_json_delta\",\"partial_json\":\"and\\\":\\\"ls\\\"}\"}}",
    "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},"
        "\"usage\":{\"output_tokens\":77}}",
    0
  };
  struct conv c = { 0, 0, 0 };
  struct reply r = { STRALLOC_ZERO, STRALLOC_ZERO, STRALLOC_ZERO, 0, { 0, 0, 0, 0 } };
  struct req q = { "m", 100, "sys", tool_all, 0 };
  stralloc o = STRALLOC_ZERO;
  struct msg *m;
  struct block *b;
  int i, ok = 1;

  m = conv_msg(&c, ROLE_USER);
  b = conv_block(m, BLK_TEXT);
  stralloc_copys(&b->text, "list files");
  m = conv_msg(&c, ROLE_ASSISTANT);
  for (i = 0; ev[i]; ++i)
    if (feed(anth_event, m, &r, ev[i]) != API_MORE) ok = 0;
  check(ok, "anth events accepted");
  check(feed(anth_event, m, &r, "{\"type\":\"message_stop\"}") == API_DONE,
      "anth message_stop");
  check(r.stop == STOP_TOOL, "anth stop reason");
  check(r.u.in == 12 && r.u.cread == 3000 && r.u.cwrite == 40 &&
      r.u.out == 77, "anth usage");
  check(m->n == 3, "anth three blocks");
  check(m->b[0].type == BLK_THINK && is(&m->b[0].text, "hmm") &&
      is(&m->b[0].aux, "sig"), "anth thinking and signature");
  check(m->b[1].type == BLK_TEXT && is(&m->b[1].text, "Hi"), "anth text");
  check(m->b[2].type == BLK_TOOL && is(&m->b[2].id, "tu_1") &&
      is(&m->b[2].name, "bash") && is(&m->b[2].text, "{\"command\":\"ls\"}"),
      "anth tool call from partial json");

  m = conv_msg(&c, ROLE_USER);
  b = conv_block(m, BLK_RESULT);
  stralloc_copys(&b->id, "tu_1");
  stralloc_copys(&b->text, "a\nb");
  q.conv = &c;
  check(anth_render(&o, &q), "anth render");
  check(has(&o, "{\"type\":\"thinking\",\"thinking\":\"hmm\","
      "\"signature\":\"sig\"}"), "anth thinking sent back unchanged");
  check(has(&o, "\"name\":\"bash\",\"input\":{\"command\":\"ls\"}"),
      "anth tool_use input");
  check(has(&o, "{\"type\":\"tool_result\",\"tool_use_id\":\"tu_1\","
      "\"content\":\"a\\nb\"}"), "anth tool_result");
  check(has(&o, "\"system\":[{\"type\":\"text\",\"text\":\"sys\","
      "\"cache_control\":{\"type\":\"ephemeral\"}}]"), "anth system, cached");
  check(has(&o, "\"stream\":true,\"cache_control\":{\"type\":\"ephemeral\"}"),
      "anth automatic caching");
  check(feed(anth_event, m, &r, "{\"type\":\"error\",\"error\":{\"type\":"
      "\"overloaded_error\",\"message\":\"Overloaded\"}}") == API_ERROR &&
      is(&r.err, "Overloaded"), "anth error event");

  /* a saved and reloaded conversation renders exactly the same */
  {
    struct conv d = { 0, 0, 0 };
    stralloc s = STRALLOC_ZERO, o2 = STRALLOC_ZERO;

    b->iserr = 1;
    stralloc_cats(&b->text, "\n\"quoted\" \\ and \xc3\xa9 and \x01");
    check(conv_save(&s, &c), "save");
    stralloc_cats(&s, "{\"not a message\":1}\n");
    check(conv_load(&d, s.s, s.len), "load");
    check(d.n == c.n && d.m[1].n == 3 && d.m[1].b[0].type == BLK_THINK &&
        is(&d.m[1].b[0].aux, "sig") && d.m[2].b[0].iserr, "load contents");
    q.conv = &c;
    anth_render(&o, &q);
    q.conv = &d;
    anth_render(&o2, &q);
    check(o.len == o2.len && !memcmp(o.s, o2.s, o.len), "round trip");
    check(!conv_load(&d, "{\"r\":7,\"b\":[]}\n", 15), "reject bad role");
  }
}

static void
openai(void)
{
  static const char *ev[] = {
    "{\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\","
        "\"content\":\"Let me\"}}]}",
    "{\"choices\":[{\"index\":0,\"delta\":{\"reasoning_content\":\"think\"}}]}",
    "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":"
        "\"read_file\",\"arguments\":\"{\\\"pa\"}}]}}]}",
    "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,"
        "\"function\":{\"arguments\":\"th\\\":\\\"x\\\"}\"}}]}}]}",
    "{\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"tool_calls\"}]}",
    0
  };
  struct conv c = { 0, 0, 0 };
  struct reply r = { STRALLOC_ZERO, STRALLOC_ZERO, STRALLOC_ZERO, 0, { 0, 0, 0, 0 } };
  struct req q = { "m", 100, "sys", tool_all, 0 };
  stralloc o = STRALLOC_ZERO;
  struct msg *m;
  struct block *b;
  int i, ok = 1;

  m = conv_msg(&c, ROLE_USER);
  b = conv_block(m, BLK_TEXT);
  stralloc_copys(&b->text, "read x");
  m = conv_msg(&c, ROLE_ASSISTANT);
  for (i = 0; ev[i]; ++i)
    if (feed(oai_event, m, &r, ev[i]) != API_MORE) ok = 0;
  check(ok, "oai chunks accepted");
  check(is(&r.think, ""), "oai reasoning only in its own chunk");
  check(feed(oai_event, m, &r, "{\"choices\":[],\"usage\":{\"prompt_tokens\":"
      "500,\"completion_tokens\":30,\"prompt_tokens_details\":"
      "{\"cached_tokens\":400}}}") == API_MORE, "oai usage chunk");
  check(r.u.in == 100 && r.u.cread == 400 && r.u.out == 30, "oai usage");
  check(feed(oai_event, m, &r, "[DONE]") == API_DONE, "oai [DONE]");
  check(r.stop == STOP_TOOL, "oai finish reason");
  check(m->n == 2 && m->b[0].type == BLK_TEXT && is(&m->b[0].text, "Let me"),
      "oai text");
  check(m->b[1].type == BLK_TOOL && is(&m->b[1].id, "call_1") &&
      is(&m->b[1].name, "read_file") && is(&m->b[1].text, "{\"path\":\"x\"}"),
      "oai tool call across chunks");

  m = conv_msg(&c, ROLE_USER);
  b = conv_block(m, BLK_RESULT);
  stralloc_copys(&b->id, "call_1");
  stralloc_copys(&b->text, "contents");
  q.conv = &c;
  check(oai_render(&o, &q), "oai render");
  check(has(&o, "{\"role\":\"system\",\"content\":\"sys\"}"), "oai system");
  check(has(&o, "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\","
      "\"function\":{\"name\":\"read_file\",\"arguments\":"
      "\"{\\\"path\\\":\\\"x\\\"}\"}}]"), "oai tool_calls");
  check(has(&o, "{\"role\":\"tool\",\"tool_call_id\":\"call_1\","
      "\"content\":\"contents\"}"), "oai tool message");
  check(has(&o, "\"parameters\":{\"type\":\"object\""), "oai tool schema");
}

int
main(void)
{
  anthropic();
  openai();
  if (!fails) printf("conv: ok\n");
  return fails ? 1 : 0;
}
