#include "agent.h"

/* the header of a session file, if it is one started in dir */
static int
indir(const char *fn, const char *dir)
{
  char buf[8192];
  const char *v;
  size_t vn, e;
  ssize_t r;
  int fd, ok = 0;

  if ((fd = open(fn, O_RDONLY | O_CLOEXEC)) == -1) return 0;
  r = read(fd, buf, sizeof buf);
  close(fd);
  if (r <= 0) return 0;
  e = byte_chr(buf, (size_t)r, '\n');
  if (json_get(buf, e, "cwd", &v, &vn) && json_unstr(&tmp, v, vn) &&
      stralloc_0(&tmp))
    ok = !str_diff(tmp.s, dir);
  return ok;
}

/* carries on with the newest session started here; 1 if there was one */
int
resume(void)
{
  stralloc d = STRALLOC_ZERO, fn = STRALLOC_ZERO, best = STRALLOC_ZERO;
  stralloc s = STRALLOC_ZERO;
  struct dirent *e;
  const struct msg *m;
  DIR *dp;
  size_t i, j, n;
  int ok = 0;

  if (!sessdir(&d) || !(dp = opendir(d.s))) goto out;
  while ((e = readdir(dp))) {
    n = str_len(e->d_name);
    if (n < 6 || str_diff(e->d_name + n - 5, ".json")) continue;
    if (best.len && str_diff(e->d_name, best.s + d.len + 1) <= 0) continue;
    if (!stralloc_copy(&fn, &d) || !stralloc_cats(&fn, "/") ||
        !stralloc_cats(&fn, e->d_name) || !stralloc_0(&fn))
      nomem();
    if (!stralloc_copys(&tmp, "")) nomem();
    if (indir(fn.s, cwdsa.s) && !stralloc_copy(&best, &fn)) nomem();
  }
  closedir(dp);
  if (!best.len) goto out;
  if (!file_get(best.s, &s)) { kwarnsys("unable to read ", best.s); goto out; }
  if (!conv_load(&conv, s.s, s.len)) {
    kwarn("unable to load the session in ", best.s, 0);
    while (conv.n) conv_drop(&conv);
    goto out;
  }
  if (!stralloc_copy(&sessfn, &best)) nomem();
  setundo();
  pending = s.len;  /* about the size of the conversation, for compacting */

  /* a reminder of where things stood */
  outs("(resumed the session in ");
  outs(best.s);
  outs(")\n");
  for (i = conv.n; i-- > 0;) {
    m = &conv.m[i];
    if (m->role != ROLE_USER) continue;
    for (j = m->n; j-- > 0;)
      if (m->b[j].type == BLK_TEXT && m->b[j].text.len) break;
    if (j == (size_t)-1) continue;
    lines("  last request: ", m->b[j].text.s, m->b[j].text.len, 1);
    break;
  }
  ok = 1;
out:
  stralloc_free(&d);
  stralloc_free(&fn);
  stralloc_free(&best);
  stralloc_free(&s);
  return ok;
}
