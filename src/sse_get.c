#include "byte.h"
#include "http.h"
#include "sse.h"
#include "str.h"
#include "stralloc.h"

/* moves one line (without its newline) from s->x into s->line */
static int
readline(struct sse *s, struct http *h)
{
  size_t i;
  ssize_t r;

  if (!stralloc_copys(&s->line, "")) return -1;
  for (;;) {
    if (s->p == s->n) {
      if (s->eof) return s->line.len ? 1 : 0;
      r = http_get(h, s->x, sizeof s->x);
      if (r < 0) return -1;
      if (r == 0) { s->eof = 1; continue; }
      s->p = 0;
      s->n = (size_t)r;
    }
    i = byte_chr(s->x + s->p, s->n - s->p, '\n');
    if (!stralloc_catb(&s->line, s->x + s->p, i)) return -1;
    s->p += i;
    if (s->p < s->n) { ++s->p; break; }
  }
  if (s->line.len && s->line.s[s->line.len - 1] == '\r') --s->line.len;
  return 1;
}

/* the value after "field:", minus one optional leading space */
static int
field(stralloc *sa, const stralloc *line, size_t skip, int append)
{
  const char *v = line->s + skip;
  size_t n = line->len - skip;

  if (n && *v == ' ') { ++v; --n; }
  if (append) {
    if (sa->len && !stralloc_append(sa, '\n')) return 0;
    return stralloc_catb(sa, v, n);
  }
  return stralloc_copyb(sa, v, n);
}

/*
 * reads the next server-sent event: its type into event (empty if
 * none was given) and its data into data, both NUL-terminated with
 * the NUL not counted.  returns 1 for an event, 0 at end of stream,
 * -1 on error.
 */
int
sse_get(struct sse *s, struct http *h, stralloc *event, stralloc *data)
{
  int r, have = 0;

  if (!stralloc_copys(event, "")) return -1;
  if (!stralloc_copys(data, "")) return -1;
  for (;;) {
    r = readline(s, h);
    if (r < 0) return -1;
    if (r == 0) break;
    if (!s->line.len) {
      if (have) break;
      continue;
    }
    if (s->line.s[0] == ':') continue;
    if (s->line.len >= 6 && !byte_diff(s->line.s, 6, "event:")) {
      if (!field(event, &s->line, 6, 0)) return -1;
      have = 1;
    } else if (s->line.len >= 5 && !byte_diff(s->line.s, 5, "data:")) {
      if (!field(data, &s->line, 5, 1)) return -1;
      have = 1;
    }
  }
  if (!have) return 0;
  if (!stralloc_0(event) || !stralloc_0(data)) return -1;
  --event->len;
  --data->len;
  return 1;
}
