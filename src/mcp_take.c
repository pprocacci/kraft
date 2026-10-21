#include "byte.h"
#include "json.h"
#include "mcp.h"
#include "stralloc.h"

/* "server's" error message, or its code */
static void
failed(const char *e, size_t n, stralloc *out)
{
  const char *v;
  size_t vn;

  stralloc_copys(out, "");
  if (json_get(e, n, "message", &v, &vn) && json_unstr(out, v, vn) && out->len)
    return;
  stralloc_copys(out, "error ");
  if (json_get(e, n, "code", &v, &vn)) stralloc_catb(out, v, vn);
}

/* answers a request from the server: a ping, or "not supported" */
static void
answer(struct mcp *m, const char *s, size_t n, const char *id, size_t idn)
{
  stralloc r = STRALLOC_ZERO;
  const char *v;
  size_t vn;
  int ping = json_get(s, n, "method", &v, &vn) && json_is(v, vn, "ping");

  if (stralloc_copys(&r, "{\"jsonrpc\":\"2.0\",\"id\":") &&
      stralloc_catb(&r, id, idn) &&
      stralloc_cats(&r, ping ? ",\"result\":{}}" : ",\"error\":{\"code\":"
          "-32601,\"message\":\"kraft does not support that\"}}"))
    mcp_send(m, r.s, r.len);
  stralloc_free(&r);
}

/*
 * deals with message s from server m while waiting for the answer to
 * request id (idn bytes of JSON).  returns 1 if it is the answer, with
 * the result in out; 0 if it is the answer and an error, with the
 * reason; -1 if it is something else (dealt with, or skipped).
 */
int
mcp_take(struct mcp *m, const char *s, size_t n, const char *id, size_t idn,
    stralloc *out)
{
  const char *v, *mid;
  size_t vn, midn;

  if (!json_object(s, n) || !json_get(s, n, "id", &mid, &midn)) return -1;
  if (json_get(s, n, "method", &v, &vn)) {
    answer(m, s, n, mid, midn);
    return -1;
  }
  if (midn != idn || byte_diff(mid, idn, id)) return -1;
  if (json_get(s, n, "result", &v, &vn)) {
    if (!stralloc_copyb(out, v, vn)) {
      stralloc_copys(out, "out of memory");
      return 0;
    }
    return 1;
  }
  if (json_get(s, n, "error", &v, &vn)) failed(v, vn, out);
  else stralloc_copys(out, "the answer had neither result nor error");
  return 0;
}
