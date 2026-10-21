#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "buffer.h"
#include "json.h"
#include "scan.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

static void
say(int fd, const char *s, size_t n)
{
  ssize_t w;

  while (n && (w = buffer_unixwrite(fd, (char *)s, n)) > 0) {
    s += w;
    n -= (size_t)w;
  }
}

/*
 * ask_user: question, optional options (an array of strings).  asks
 * on the terminal; the answer may be an option's number or anything
 * the user types.
 */
int
tool_ask(const char *in, size_t n, stralloc *out)
{
  stralloc q = STRALLOC_ZERO, opt = STRALLOC_ZERO, key = STRALLOC_ZERO;
  stralloc ans = STRALLOC_ZERO;
  const char *v;
  size_t vn, e;
  unsigned long k, pick;
  char buf[4096];
  ssize_t r;
  int fd, ok = 0;

  if (!tool_arg(in, n, "question", &q)) {
    stralloc_copys(out, "question is required");
    return 0;
  }
  buffer_flush(buffer_1);
  if ((fd = open("/dev/tty", O_RDWR | O_CLOEXEC)) == -1) {
    stralloc_copys(out, "there is no terminal to ask on; make a sensible "
        "choice yourself and say what you assumed");
    goto out;
  }
  say(fd, "\n? ", 3);
  say(fd, q.s, q.len);
  say(fd, "\n", 1);
  for (k = 0;; ++k) {
    if (!stralloc_copys(&key, "options.") || !stralloc_catulong(&key, k) ||
        !stralloc_0(&key))
      goto nomem;
    if (!json_get(in, n, key.s, &v, &vn) || *v != '"') break;
    if (!stralloc_copys(&opt, "") || !json_unstr(&opt, v, vn)) goto nomem;
    if (!stralloc_copys(&ans, "  ") || !stralloc_catulong(&ans, k + 1) ||
        !stralloc_cats(&ans, ") ") || !stralloc_cat(&ans, &opt) ||
        !stralloc_cats(&ans, "\n"))
      goto nomem;
    say(fd, ans.s, ans.len);
  }
  say(fd, k ? "  answer (a number, or your own words): " : "  answer: ",
      k ? 41 : 10);
  r = read(fd, buf, sizeof buf);  /* not retried: ^C means no answer */
  if (r <= 0) {
    say(fd, "\n", 1);
    close(fd);
    stralloc_copys(out, "the user did not answer");
    goto out;
  }
  close(fd);
  for (e = (size_t)r; e && (buf[e - 1] == '\n' || buf[e - 1] == '\r'); --e) ;
  if (!stralloc_copyb(&ans, buf, e) || !stralloc_0(&ans)) goto nomem;
  --ans.len;

  /* a number picks an option */
  if (k && ans.len && scan_ulong(ans.s, &pick) == ans.len && pick >= 1 &&
      pick <= k) {
    if (!stralloc_copys(&key, "options.") || !stralloc_catulong(&key, pick - 1)
        || !stralloc_0(&key) || !json_get(in, n, key.s, &v, &vn) ||
        !stralloc_copys(&ans, "") || !json_unstr(&ans, v, vn))
      goto nomem;
  }
  if (!stralloc_copys(out, ans.len ? "the user answered: " :
      "the user gave an empty answer") || !stralloc_cat(out, &ans))
    goto nomem;
  ok = 1;
  goto out;
nomem:
  stralloc_copys(out, "out of memory");
out:
  stralloc_free(&q);
  stralloc_free(&opt);
  stralloc_free(&key);
  stralloc_free(&ans);
  return ok;
}
