#include "byte.h"
#include "conv.h"
#include "json.h"
#include "scan.h"
#include "stralloc.h"

static int
num(const char *d, size_t n, const char *path, unsigned long *u)
{
  const char *v;
  size_t vn;

  return json_get(d, n, path, &v, &vn) && scan_ulong(v, u) == vn;
}

static int
str(const char *d, size_t n, const char *path, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(d, n, path, &v, &vn)) return 1;  /* left out: empty */
  return *v == '"' && json_unstr(out, v, vn);
}

/* one line of conv_save's output */
static int
message(struct conv *c, const char *d, size_t n)
{
  static stralloc p;
  struct msg *m;
  struct block *b;
  const char *v;
  size_t vn;
  unsigned long role, type, e, k;

  if (!num(d, n, "r", &role) || role > ROLE_ASSISTANT) return 0;
  /* conv_msg would merge consecutive user messages; keep them apart */
  if (c->n && c->m[c->n - 1].role == ROLE_USER && role == ROLE_USER) {
    if (!(m = conv_msg(c, ROLE_ASSISTANT))) return 0;
    m->role = ROLE_USER;
  } else if (!(m = conv_msg(c, (int)role)))
    return 0;
  for (k = 0;; ++k) {
    if (!stralloc_copys(&p, "b.") || !stralloc_catulong(&p, k) ||
        !stralloc_0(&p))
      return 0;
    if (!json_get(d, n, p.s, &v, &vn)) break;
    if (!num(v, vn, "t", &type) || type > BLK_MEDIA) return 0;
    if (!(b = conv_block(m, (int)type))) return 0;
    if (num(v, vn, "e", &e) && e) b->iserr = 1;
    if (!str(v, vn, "id", &b->id) || !str(v, vn, "name", &b->name) ||
        !str(v, vn, "text", &b->text) || !str(v, vn, "aux", &b->aux))
      return 0;
  }
  return 1;
}

/*
 * appends the messages in s, as written by conv_save, to c.  lines
 * that are not messages (a header, say) are skipped.  returns 1 on
 * success, 0 if a message line is malformed or memory runs out.
 */
int
conv_load(struct conv *c, const char *s, size_t n)
{
  size_t i, e;

  for (i = 0; i < n; i = e + 1) {
    e = i + byte_chr(s + i, n - i, '\n');
    if (e - i > 4 && !byte_diff(s + i, 5, "{\"r\":") &&
        !message(c, s + i, e - i))
      return 0;
  }
  return 1;
}
