#include <stdio.h>
#include <string.h>
#include "diff.h"
#include "json.h"
#include "md.h"
#include "media.h"
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

  {
    stralloc b = STRALLOC_ZERO;

    base64_enc(&b, "Man", 3);
    base64_enc(&b, "Ma", 2);
    base64_enc(&b, "M", 1);
    base64_enc(&b, "\377\376\375", 3);
    check(b.len == 16 && !memcmp(b.s, "TWFuTWE=TQ==//79", 16), "base64");
    check(media_type("\211PNG\r\n\032\nxxxx", 12) &&
        !strcmp(media_type("\211PNG\r\n\032\nxxxx", 12), "image/png") &&
        !strcmp(media_type("%PDF-1.4", 8), "application/pdf") &&
        !media_type("hello, world", 12), "media types");
  }

  {
    /* Markdown renders the same however the text is cut up */
    static const char doc[] = "# Title\nsome **bold** and `x*y`, 2*3\n"
        "```c\nint a; `q`\n```\nafter\n";
    struct md m1 = MD_ZERO, m2 = MD_ZERO;
    stralloc whole = STRALLOC_ZERO, bits = STRALLOC_ZERO;
    size_t i;

    md_put(&m1, &whole, doc, sizeof doc - 1);
    md_end(&m1, &whole);
    for (i = 0; i < sizeof doc - 1; ++i) md_put(&m2, &bits, doc + i, 1);
    md_end(&m2, &bits);
    stralloc_0(&whole);
    check(whole.len == bits.len + 1 && !memcmp(whole.s, bits.s, bits.len),
        "md: pieces render as the whole");
    check(strstr(whole.s, "\033[1mTitle") && !strstr(whole.s, "# Title"),
        "md: heading");
    check(strstr(whole.s, "\033[1mbold\033[0m") != 0, "md: bold");
    check(strstr(whole.s, "\033[36mx*y\033[0m") != 0, "md: code keeps its *");
    check(strstr(whole.s, " 2*3") != 0, "md: lone star");
    check(strstr(whole.s, "\033[36mint a; `q`") != 0, "md: fenced block");
    check(strstr(whole.s, "after") != 0, "md: after the fence");
  }

  {
    /* line diffs */
    static const char a[] = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";
    static const char b[] = "1\n2\n3\nfour\n5\n6\n7\n8\n9\n10\n11\n";
    stralloc d = STRALLOC_ZERO;

    diff_lines(&d, a, sizeof a - 1, b, sizeof b - 1, 1);
    stralloc_0(&d);
    check(!strcmp(d.s, "@@ 3\n 3\n-4\n+four\n 5\n@@ 10\n 10\n+11\n"),
        "diff: two hunks with context");
    stralloc_copys(&d, "");
    diff_lines(&d, a, sizeof a - 1, a, sizeof a - 1, 3);
    check(d.len == 0, "diff: no changes, no output");
    stralloc_copys(&d, "");
    diff_lines(&d, "", 0, "new\nfile", 8, 3);
    stralloc_0(&d);
    check(!strcmp(d.s, "@@ 1\n+new\n+file\n"), "diff: a new file");
    stralloc_copys(&d, "");
    diff_lines(&d, "x\ny\n", 4, "", 0, 3);
    stralloc_0(&d);
    check(!strcmp(d.s, "@@ 1\n-x\n-y\n"), "diff: everything removed");
  }

  if (!fails) printf("json: ok\n");
  return fails ? 1 : 0;
}
