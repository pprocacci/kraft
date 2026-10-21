#include "agent.h"

/* appends s to out with the glob characters in it made literal */
static void
literal(stralloc *out, const char *s)
{
  for (; *s; ++s) {
    if ((*s == '*' || *s == '?' || *s == '[' || *s == '\\') &&
        !stralloc_append(out, '\\'))
      nomem();
    if (!stralloc_append(out, *s)) nomem();
  }
}

/*
 * the "permanently" answer: a rule allowing just this call goes in
 * the project's .kraft/config, which stays trusted.  returns 1 if a
 * rule was saved.
 */
int
saverule(const struct tool *t, const char *in, size_t n)
{
  stralloc r = STRALLOC_ZERO, f = STRALLOC_ZERO, hash = STRALLOC_ZERO;
  const char *s;
  size_t k;
  int fd, ok = 0;

  if (!stralloc_copys(&r, "allow ") || !stralloc_cats(&r, isedit(t->name) ?
      "edit" : t->name) || !stralloc_cats(&r, " "))
    nomem();
  if (!str_diff(t->name, "bash")) {
    if (!tool_arg(in, n, "command", &tmp) || !plain(tmp.s)) {
      styled(YELLOW, "  (only a plain command can be saved as a rule; "
          "allowed this once)");
      outs("\n");
      goto out;
    }
    literal(&r, tmp.s);
  } else if (isedit(t->name)) {
    if (!tool_arg(in, n, "path", &tmp)) goto out;
    relpath(tmp.s, &f);
    if (str_start(f.s, ".kraft/") || hits("*/.kraft/*", f.s, 0)) goto out;
    literal(&r, f.s);
  } else if (!str_diff(t->name, "web_fetch")) {
    if (!tool_arg(in, n, "url", &tmp)) goto out;
    /* the whole site: scheme, host and port */
    s = tmp.s + (str_start(tmp.s, "https://") ? 8 : 7);
    k = byte_chr(s, str_len(s), '/');
    if (!stralloc_copyb(&f, tmp.s, (size_t)(s - tmp.s) + k) || !stralloc_0(&f))
      nomem();
    literal(&r, f.s);
    if (!stralloc_cats(&r, "/*")) nomem();
  } else if (!stralloc_cats(&r, "*")) nomem();
  if (!stralloc_cats(&r, "\n")) nomem();

  if (!stralloc_copyb(&f, projcfg.s, str_len(projcfg.s) - 7) || !stralloc_0(&f))
    nomem();
  mkdir(f.s, 0755);
  fd = open(projcfg.s, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
  if (fd == -1 || buffer_unixwrite(fd, r.s, r.len) != (ssize_t)r.len) {
    kwarnsys("unable to add a rule to ", projcfg.s);
    if (fd != -1) close(fd);
    goto out;
  }
  close(fd);
  /* we wrote it, so it stays trusted */
  if (file_get(projcfg.s, &f) && stralloc_copys(&hash, "") &&
      sha256_hex(&hash, f.s, f.len) && stralloc_0(&hash))
    trust(projcfg.s, hash.s);
  --r.len;  /* without the newline */
  if (!stralloc_0(&r)) nomem();
  cfgfile = sdup(projcfg.s);
  setting(0, r.s[0] == 'a' ? "allow" : "deny", r.s + 6, 0);
  sgr(DIM);
  outs("  (saved in ");
  outs(projcfg.s);
  outs(": ");
  outs(r.s);
  outs(")");
  sgr(0);
  outs("\n");
  ok = 1;
out:
  stralloc_free(&r);
  stralloc_free(&f);
  stralloc_free(&hash);
  return ok;
}
