#include "agent.h"

/* fetches p from the server into out; 1 if it answered 200 */
static int
getjson(const char *p, stralloc *out)
{
  char buf[4096];
  ssize_t n = -1;
  int fd;

  fd = tcp_connect(url.host.s, url.port.s, CONNECTSECS);
  if (fd == -1) return 0;
  if (url.tls && (fd = tls_connect(fd, url.host.s, &tlswhy)) == -1) return 0;
  curfd = fd;
  if (http_request(fd, "GET", host.s, p, ghdrs.s, 0, 0) &&
      http_response(&http, fd) > 0 && http.status == 200) {
    if (!stralloc_copys(out, "")) nomem();
    while ((n = http_get(&http, buf, sizeof buf)) > 0)
      if (out->len < 1048576 && !stralloc_catb(out, buf, (size_t)n)) nomem();
  }
  curfd = -1;
  close(fd);
  return n == 0;
}

/*
 * asks the server for the model's context window: the Anthropic
 * Models API's max_input_tokens, or LM Studio's loaded context.
 * leaves ctxwin 0 if neither answers.
 */
void
findwindow(void)
{
  static const char *const where[][2] = {
    { "/v1/models/", "max_input_tokens" },
    { "/api/v0/models/", "loaded_context_length" },
    { "/api/v0/models/", "max_context_length" },
  };
  stralloc p = STRALLOC_ZERO, j = STRALLOC_ZERO;
  const char *v;
  size_t i, vn;
  unsigned long u;

  for (i = 0; i < sizeof where / sizeof where[0] && !ctxwin && !intr; ++i) {
    if (!stralloc_copy(&p, &pfx) || !stralloc_cats(&p, where[i][0]) ||
        !stralloc_cats(&p, model) || !stralloc_0(&p))
      nomem();
    if (!getjson(p.s, &j)) continue;
    if (json_get(j.s, j.len, where[i][1], &v, &vn) &&
        scan_ulongb(v, vn, &u) == vn && u)
      ctxwin = u;
  }
  stralloc_free(&p);
  stralloc_free(&j);
}
