/*
 * A libFuzzer harness for the code that reads untrusted input: what
 * comes from the network (HTTP, SSE, JSON, the two stream formats),
 * from disk (saved sessions, config files) and from the model (the
 * commands that auto mode judges, edits) and from MCP servers.  The
 * first byte picks the target.  Some targets also check properties,
 * and abort if one fails, so they find wrong answers as well as
 * crashes.
 *
 *   make fuzz && ./t/fuzz -max_total_time=600 t/corpus
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "anth.h"
#include "api.h"
#include "cfg.h"
#include "conv.h"
#include "diff.h"
#include "edit.h"
#include "html.h"
#include "http.h"
#include "json.h"
#include "mcp.h"
#include "md.h"
#include "media.h"
#include "oai.h"
#include "oauth.h"
#include "perm.h"
#include "sse.h"
#include "stralloc.h"
#include "tool.h"
#include "url.h"

#define MAXIN 60000  /* the largest input tried */

static stralloc a, b, c;

static void
json(const char *s, size_t n)
{
  static const char *const paths[] = {
    "type", "delta.text", "a.b.0.c", "choices.0.delta.tool_calls.0.id",
    "error.message", "0", "x.1.y.2", ""
  };
  const char *v;
  size_t vn, i;

  json_skip(s, n, 0);
  json_object(s, n);
  for (i = 0; i < sizeof paths / sizeof paths[0]; ++i)
    if (json_get(s, n, paths[i], &v, &vn)) {
      if (v < s || v + vn > s + n) abort();  /* a value inside the input */
      stralloc_copys(&a, "");
      json_unstr(&a, v, vn);
    }
  /* what json_str writes, json_unstr reads back the same */
  stralloc_copys(&a, "");
  stralloc_copys(&b, "");
  json_str(&a, s, n);
  if (!json_unstr(&b, a.s, a.len) || b.len != n || memcmp(b.s, s, n)) abort();
  if (json_skip(a.s, a.len, 0) != a.len) abort();
}

static void
http(const char *s, size_t n)
{
  static struct http h;
  struct sse e = SSE_ZERO;
  int p[2];

  if (n > 8000) n = 8000;  /* within the socket buffer: no reader yet */
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, p) == -1) return;
  if (write(p[1], s, n) != (ssize_t)n) { close(p[0]); close(p[1]); return; }
  close(p[1]);
  if (http_response(&h, p[0]) > 0)
    while (sse_get(&e, &h, &a, &b) > 0) ;
  close(p[0]);
  stralloc_free(&e.line);
}

/* events separated by newlines, all into one message */
static void
events(int (*ev)(struct msg *, const char *, size_t, struct reply *),
    const char *s, size_t n)
{
  static struct reply r;
  struct conv cv = { 0, 0, 0 };
  struct msg *m = conv_msg(&cv, ROLE_ASSISTANT);
  size_t i, e;

  for (i = 0; m && i < n; i = e + 1) {
    for (e = i; e < n && s[e] != '\n'; ++e) ;
    stralloc_copys(&r.text, "");
    stralloc_copys(&r.think, "");
    stralloc_copys(&r.err, "");
    ev(m, s + i, e - i, &r);
  }
  while (cv.n) conv_drop(&cv);
  free(cv.m);
}

/* a session read back: saved and read again, it is the same */
static void
session(const char *s, size_t n)
{
  struct conv x = { 0, 0, 0 }, y = { 0, 0, 0 };

  if (conv_load(&x, s, n)) {
    stralloc_copys(&a, "");
    stralloc_copys(&b, "");
    conv_save(&a, &x);
    if (!conv_load(&y, a.s, a.len)) abort();
    conv_save(&b, &y);
    if (a.len != b.len || memcmp(a.s, b.s, a.len)) abort();
  }
  while (x.n) conv_drop(&x);
  while (y.n) conv_drop(&y);
  free(x.m);
  free(y.m);
}

/* Markdown cut anywhere renders as it does whole */
static void
markdown(const char *s, size_t n)
{
  struct md m1 = MD_ZERO, m2 = MD_ZERO;
  size_t i, step;

  stralloc_copys(&a, "");
  stralloc_copys(&b, "");
  md_put(&m1, &a, s, n);
  md_end(&m1, &a);
  step = n ? (unsigned char)s[0] % 7 + 1 : 1;
  for (i = 0; i < n; i += step)
    md_put(&m2, &b, s + i, n - i < step ? n - i : step);
  md_end(&m2, &b);
  if (a.len != b.len || memcmp(a.s, b.s, a.len)) abort();
}

static int
setting(void *arg, const char *k, const char *v, unsigned long line)
{
  (void)arg; (void)k; (void)v; (void)line;
  return 1;
}

/* three parts, split at the first two NULs */
static int
parts(const char *s, size_t n, size_t *p1, size_t *p2)
{
  const char *x = memchr(s, 0, n), *y;

  if (!x) return 0;
  y = memchr(x + 1, 0, n - (size_t)(x + 1 - s));
  if (!y) return 0;
  *p1 = (size_t)(x - s);
  *p2 = (size_t)(y - s);
  return 1;
}

static void
edits(const char *s, size_t n)
{
  size_t x, y;
  int all = n && (s[n - 1] & 1);

  if (!parts(s, n, &x, &y)) return;
  stralloc_copyb(&a, s, x);
  stralloc_copyb(&b, s + x + 1, y - x - 1);
  stralloc_copyb(&c, s + y + 1, n - y - 1);
  edit_apply(&a, &b, &c, all, &b);
}

static void
diffs(const char *s, size_t n)
{
  const char *x = memchr(s, 0, n);
  size_t k = x ? (size_t)(x - s) : n / 2;

  stralloc_copys(&a, "");
  diff_lines(&a, s, k, s + k + (x != 0), n - k - (x != 0), (unsigned)(n % 5));
}

static void
command(const char *s, size_t n)
{
  /* as the model would send it: {"command": ...} */
  stralloc_copys(&b, "{\"command\":");
  json_str(&b, s, n);
  stralloc_cats(&b, "}");
  stralloc_copys(&c, "");
  perm_auto("bash", b.s, b.len, &c);
  stralloc_copys(&b, "{\"path\":");
  json_str(&b, s, n);
  stralloc_cats(&b, "}");
  perm_auto("edit_file", b.s, b.len, &c);
}

/* a message from an MCP server, perhaps a remote one */
static void
mcpmsg(const char *s, size_t n)
{
  static struct mcp m;  /* not up: an answer to the server goes nowhere */

  mcp_take(&m, s, n, "7", 1, &a);
  /* a WWW-Authenticate challenge; a redirect, pasted or not */
  www_param(s, n, "resource_metadata", &a);
  www_param(s, n, "scope", &a);
  url_param(s, n, "code", &a);
  url_param(s, n, "state", &a);
}

int
LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
  static struct url u;
  const char *s = (const char *)d + 1;

  if (!n || n > MAXIN) return 0;
  --n;
  switch (d[0] % 13) {
  case 0: json(s, n); break;
  case 1: http(s, n); break;
  case 2: events(anth_event, s, n); break;
  case 3: events(oai_event, s, n); break;
  case 4: session(s, n); break;
  case 5: stralloc_copys(&a, ""); html_text(&a, s, n); break;
  case 6: markdown(s, n); break;
  case 7: cfg_parse(s, n, setting, 0); break;
  case 8: command(s, n); break;
  case 9:
    stralloc_copyb(&a, s, n);
    stralloc_0(&a);
    if (!memchr(s, 0, n)) url_parse(&u, a.s);
    break;
  case 10: edits(s, n); break;
  case 11:
    diffs(s, n);
    media_type(s, n);
    stralloc_copys(&a, "");
    base64_enc(&a, s, n);
    break;
  case 12: mcpmsg(s, n); break;
  }
  return 0;
}
