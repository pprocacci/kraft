#include <stdio.h>  /* rename */
#include "agent.h"

/* "key":value, after a comma if need be */
static void
pair(stralloc *s, const char *k, const stralloc *v)
{
  if (s->len > 1 && !stralloc_cats(s, ",")) nomem();
  if (!json_str(s, k, str_len(k)) || !stralloc_cats(s, ":") ||
      !json_str(s, v->s ? v->s : "", v->len))
    nomem();
}

/*
 * saves login c for server k, readable only by the user: it is as
 * good as a password.  returns 1 if it was saved.
 */
int
mcpcredsave(unsigned long k, const struct mcpcred *c)
{
  static stralloc fn, tmpf, s;
  size_t i;
  int fd, ok;

  mcpcredfn(k, &fn);
  /* the directory: ~/.kraft/mcp-auth */
  if (!sessdir(&s)) return 0;  /* makes ~/.kraft */
  i = fn.len - 1;
  while (i && fn.s[i] != '/') --i;
  if (!stralloc_copyb(&tmpf, fn.s, i) || !stralloc_0(&tmpf)) nomem();
  mkdir(tmpf.s, 0700);

  if (!stralloc_copys(&s, "{")) nomem();
  pair(&s, "url", &c->url);
  pair(&s, "issuer", &c->issuer);
  pair(&s, "token_endpoint", &c->tokenep);
  pair(&s, "client_id", &c->clientid);
  pair(&s, "client_secret", &c->secret);
  pair(&s, "auth_method", &c->method);
  pair(&s, "access_token", &c->access);
  pair(&s, "refresh_token", &c->refresh);
  if (!stralloc_cats(&s, ",\"expires_at\":") ||
      !stralloc_catulong(&s, c->expires) || !stralloc_cats(&s, "}\n"))
    nomem();

  if (!stralloc_copyb(&tmpf, fn.s, fn.len - 1) || !stralloc_cats(&tmpf, ".new") ||
      !stralloc_0(&tmpf))
    nomem();
  unlink(tmpf.s);
  fd = open(tmpf.s, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  ok = fd != -1 && buffer_unixwrite(fd, s.s, s.len) == (ssize_t)s.len &&
      fsync(fd) == 0;
  if (fd != -1) close(fd);
  ok = ok && rename(tmpf.s, fn.s) == 0;
  if (!ok) {
    kwarnsys("unable to save the login in ", fn.s);
    unlink(tmpf.s);
  }
  return ok;
}
