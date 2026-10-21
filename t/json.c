#include <stdio.h>
#include <string.h>
#include "json.h"
#include "stralloc.h"

static int fails;

static void
check(int ok, const char *what)
{
  if (!ok) { printf("FAIL %s\n", what); ++fails; }
}

/* looks up path in doc and decodes it as a string */
static int
getstr(const char *doc, const char *path, const char *want)
{
  stralloc out = STRALLOC_ZERO;
  const char *v;
  size_t vn;
  int ok;

  if (!json_get(doc, strlen(doc), path, &v, &vn)) return 0;
  if (!json_unstr(&out, v, vn)) return 0;
  ok = out.len == strlen(want) && !memcmp(out.s, want, out.len);
  stralloc_free(&out);
  return ok;
}

int
main(void)
{
  const char *anth = "{\"type\":\"content_block_delta\",\"index\":0,"
      "\"delta\":{\"type\":\"text_delta\",\"text\":\"Hi \\\"you\\\"\\n\"}}";
  const char *oai = "{ \"id\": \"x\", \"choices\": [ { \"index\": 0, "
      "\"delta\": { \"content\": \"caf\\u00e9 \\ud83d\\ude00\" } } ] }";
  const char *v;
  size_t vn;
  stralloc sa = STRALLOC_ZERO;

  check(getstr(anth, "type", "content_block_delta"), "anth type");
  check(getstr(anth, "delta.text", "Hi \"you\"\n"), "anth text");
  check(!json_get(anth, strlen(anth), "delta.nope", &v, &vn), "missing key");
  check(getstr(oai, "choices.0.delta.content", "caf\xc3\xa9 \xf0\x9f\x98\x80"),
      "oai utf-8 and surrogates");
  check(!json_get(oai, strlen(oai), "choices.1", &v, &vn), "past array end");
  check(json_get(oai, strlen(oai), "choices.0.index", &v, &vn) &&
      vn == 1 && *v == '0', "number value");

  v = "[1, {\"a\": [true, null]}, \"x\"]";
  check(json_skip(v, strlen(v), 0) == strlen(v), "skip nested");
  check(!json_skip("{\"a\": }", 7, 0), "reject bad object");
  check(!json_skip("\"open", 5, 0), "reject open string");

  check(json_str(&sa, "a\"b\\c\n\x01", 7) && sa.len == 17 &&
      !memcmp(sa.s, "\"a\\\"b\\\\c\\n\\u0001\"", 17), "encode");

  if (!fails) printf("json: ok\n");
  return fails ? 1 : 0;
}
