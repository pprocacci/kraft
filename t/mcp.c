/* the MCP client against t/mcpd */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include "json.h"
#include "mcp.h"
#include "stralloc.h"

static int fails;
static volatile sig_atomic_t stop;

static void
expect(struct mcp *m, const char *method, const char *params, int ok,
    const char *want)
{
  stralloc out = STRALLOC_ZERO;
  int got = mcp_rpc(m, method, params, &out, 10, &stop);

  stralloc_0(&out);
  if (got != ok || !strstr(out.s, want)) {
    printf("FAIL %s %s: got %d %s, want %d %s\n", method,
        params ? params : "", got, out.s, ok, want);
    ++fails;
  }
  stralloc_free(&out);
}

int
main(void)
{
  struct mcp m;
  stralloc why = STRALLOC_ZERO;

  signal(SIGPIPE, SIG_IGN);
  memset(&m, 0, sizeof m);
  m.name = "mcpd";
  m.cmd = "./t/mcpd";
  if (!mcp_start(&m, 0, &why)) {
    printf("FAIL start\n");
    return 1;
  }
  expect(&m, "initialize", "{}", 1, "\"serverInfo\"");
  expect(&m, "tools/list", "{}", 1, "\"nextCursor\":\"p2\"");
  expect(&m, "tools/list", "{\"cursor\":\"p2\"}", 1, "\"fail\"");
  expect(&m, "tools/call", "{\"name\":\"echo\",\"arguments\":{\"text\":"
      "\"hi \\\"there\\\"\"}}", 1, "hi \\\"there\\\"");
  expect(&m, "tools/call", "{\"name\":\"add.two\",\"arguments\":{\"a\":2,"
      "\"b\":40}}", 1, "\"42\"");
  expect(&m, "tools/call", "{\"name\":\"fail\",\"arguments\":{}}", 1,
      "\"isError\":true");
  expect(&m, "nonesuch", 0, 0, "no such method");
  mcp_stop(&m);
  expect(&m, "tools/list", "{}", 0, "not running");

  m.cmd = "exit 0";
  mcp_start(&m, 0, &why);
  expect(&m, "initialize", "{}", 0, "");
  mcp_stop(&m);

  if (fails) return 1;
  printf("mcp: ok\n");
  return 0;
}
