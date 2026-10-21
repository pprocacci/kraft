/*
 * a toy MCP server for the tests: tools echo, add and fail, listed
 * over two pages.  it chatters (a log notification, a ping of its
 * own) before answering, as real servers may.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include "stralloc.h"

static void
reply(const char *id, size_t idn, const char *result)
{
  printf("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\","
      "\"params\":{\"level\":\"info\",\"data\":\"hello\"}}\n");
  printf("{\"jsonrpc\":\"2.0\",\"id\":\"s1\",\"method\":\"ping\"}\n");
  printf("{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"result\":%s}\n", (int)idn, id,
      result);
  fflush(stdout);
}

static void
text(const char *id, size_t idn, const char *s, size_t n, int err)
{
  stralloc r = STRALLOC_ZERO;

  stralloc_copys(&r, "{\"content\":[{\"type\":\"text\",\"text\":");
  json_str(&r, s, n);
  stralloc_cats(&r, err ? "}],\"isError\":true}" : "},{\"type\":\"image\","
      "\"data\":\"AAAA\",\"mimeType\":\"image/png\"}]}");
  stralloc_0(&r);
  reply(id, idn, r.s);
  stralloc_free(&r);
}

int
main(void)
{
  char buf[65536];
  stralloc s = STRALLOC_ZERO;
  const char *id, *m, *v;
  size_t idn, mn, vn, n;
  unsigned long a, b;

  while (fgets(buf, sizeof buf, stdin)) {
    n = strlen(buf);
    if (n && buf[n - 1] == '\n') --n;
    if (!json_get(buf, n, "method", &m, &mn)) continue;  /* our ping's answer */
    if (!json_get(buf, n, "id", &id, &idn)) continue;  /* a notification */
    if (json_is(m, mn, "initialize"))
      reply(id, idn, "{\"protocolVersion\":\"2025-06-18\",\"capabilities\":"
          "{\"tools\":{}},\"serverInfo\":{\"name\":\"mcpd\",\"version\":\"1\"}}");
    else if (json_is(m, mn, "tools/list") &&
        !json_get(buf, n, "params.cursor", &v, &vn))
      reply(id, idn, "{\"tools\":[{\"name\":\"echo\",\"description\":"
          "\"Echo the text back.\",\"inputSchema\":{\"type\":\"object\","
          "\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":"
          "[\"text\"]}},{\"name\":\"add.two\",\"description\":\"Add a and b."
          "\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"a\":"
          "{\"type\":\"integer\"},\"b\":{\"type\":\"integer\"}}}}],"
          "\"nextCursor\":\"p2\"}");
    else if (json_is(m, mn, "tools/list"))
      reply(id, idn, "{\"tools\":[{\"name\":\"fail\",\"inputSchema\":"
          "{\"type\":\"object\"}}]}");
    else if (json_is(m, mn, "tools/call") &&
        json_get(buf, n, "params.name", &v, &vn) && json_is(v, vn, "echo")) {
      stralloc_copys(&s, "");
      if (json_get(buf, n, "params.arguments.text", &v, &vn))
        json_unstr(&s, v, vn);
      text(id, idn, s.s ? s.s : "", s.len, 0);
    } else if (json_is(m, mn, "tools/call") &&
        json_get(buf, n, "params.name", &v, &vn) && json_is(v, vn, "add.two")) {
      a = b = 0;
      if (json_get(buf, n, "params.arguments.a", &v, &vn)) a = strtoul(v, 0, 10);
      if (json_get(buf, n, "params.arguments.b", &v, &vn)) b = strtoul(v, 0, 10);
      snprintf(buf, sizeof buf, "%lu", a + b);
      text(id, idn, buf, strlen(buf), 0);
    } else if (json_is(m, mn, "tools/call"))
      text(id, idn, "it failed", 9, 1);
    else {
      printf("{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"error\":{\"code\":-32601,"
          "\"message\":\"no such method\"}}\n", (int)idn, id);
      fflush(stdout);
    }
  }
  return 0;
}
