#include "agent.h"

static const char *const keys[] = {
  "url", "issuer", "token_endpoint", "client_id", "client_secret",
  "auth_method", "access_token", "refresh_token", 0
};

/*
 * server k's saved login into c (all of it emptied first).  returns
 * 1 only if there is one and it is for the server's URL as it is
 * now: a login is never sent anywhere else.
 */
int
mcpcredload(unsigned long k, struct mcpcred *c)
{
  static stralloc fn, f;
  stralloc *const field[] = {
    &c->url, &c->issuer, &c->tokenep, &c->clientid, &c->secret, &c->method,
    &c->access, &c->refresh
  };
  const char *v;
  size_t vn, i;
  unsigned long u;

  for (i = 0; keys[i]; ++i)
    if (!stralloc_copys(field[i], "")) nomem();
  c->expires = 0;
  mcpcredfn(k, &fn);
  if (!file_get(fn.s, &f) || !json_object(f.s, f.len)) return 0;
  for (i = 0; keys[i]; ++i) {
    if (json_get(f.s, f.len, keys[i], &v, &vn) && *v == '"' &&
        !json_unstr(field[i], v, vn))
      nomem();
    /* usable as strings, the NUL not counted */
    if (!stralloc_0(field[i])) nomem();
    --field[i]->len;
  }
  if (json_get(f.s, f.len, "expires_at", &v, &vn) &&
      scan_ulongb(v, vn, &u) == vn)
    c->expires = u;
  return c->url.len == str_len(mcps[k].cmd) &&
      !byte_diff(c->url.s, c->url.len, mcps[k].cmd);
}
