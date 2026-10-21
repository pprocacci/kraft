#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "http.h"
#include "sse.h"
#include "stralloc.h"

/* feeds a canned chunked HTTP response through a pipe */
static const char resp[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/event-stream\r\n"
    "Transfer-Encoding: chunked\r\n"
    "\r\n"
    "1e\r\n"
    "event: ping\r\ndata: {\"a\":1}\r\n\r\n"
    "\r\n"
    "f\r\n"
    ": comment\ndata:\r\n"
    "f\r\n"
    " x\ndata: y\n\ndat\r\n"
    "b\r\n"
    "a: [DONE]\n\n\r\n"
    "0\r\n\r\n";

static int fails;

static void
check(int ok, const char *what)
{
  if (!ok) { printf("FAIL %s\n", what); ++fails; }
}

int
main(void)
{
  static struct http h;
  struct sse s = SSE_ZERO;
  stralloc ev = STRALLOC_ZERO, data = STRALLOC_ZERO;
  int p[2];

  if (pipe(p) == -1) return 2;
  if (write(p[1], resp, sizeof resp - 1) != (ssize_t)(sizeof resp - 1))
    return 2;
  close(p[1]);

  check(http_response(&h, p[0]) == 1, "response");
  check(h.status == 200 && h.chunked, "status and chunked");

  check(sse_get(&s, &h, &ev, &data) == 1, "event 1");
  check(!strcmp(ev.s, "ping") && !strcmp(data.s, "{\"a\":1}"), "event 1 body");
  check(sse_get(&s, &h, &ev, &data) == 1, "event 2");
  check(!ev.len && !strcmp(data.s, "x\ny"), "multi-line data across chunks");
  check(sse_get(&s, &h, &ev, &data) == 1, "event 3");
  check(!strcmp(data.s, "[DONE]"), "done marker");
  check(sse_get(&s, &h, &ev, &data) == 0, "end of stream");

  if (!fails) printf("sse: ok\n");
  return fails ? 1 : 0;
}
