#include "agent.h"

/* records fn, with this hash, as trusted, in place of any older entry */
void
trust(const char *fn, const char *hash)
{
  stralloc t = STRALLOC_ZERO, u = STRALLOC_ZERO, name = STRALLOC_ZERO;
  size_t i, e, k;
  int fd;

  if (!sessdir(&name)) return;
  name.len -= str_len("/sessions");
  if (!stralloc_cats(&name, "/trusted") || !stralloc_0(&name)) nomem();
  if (file_get(name.s, &t))
    for (i = 0; i < t.len; i = e + 1) {
      e = i + byte_chr(t.s + i, t.len - i, '\n');
      k = i + byte_chr(t.s + i, e - i, ' ');
      if (k < e && e - k - 1 == str_len(fn) && !byte_diff(t.s + k + 1, e - k - 1, fn))
        continue;
      if (e > i && (!stralloc_catb(&u, t.s + i, e - i) ||
          !stralloc_cats(&u, "\n")))
        nomem();
    }
  if (!stralloc_cats(&u, hash) || !stralloc_cats(&u, " ") ||
      !stralloc_cats(&u, fn) || !stralloc_cats(&u, "\n"))
    nomem();
  if ((fd = open(name.s, O_WRONLY | O_CREAT | O_CLOEXEC, 0600)) != -1) close(fd);
  if (!file_put(name.s, u.s, u.len)) kwarnsys("unable to write ", name.s);
  stralloc_free(&t);
  stralloc_free(&u);
  stralloc_free(&name);
}
