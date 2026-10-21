#include "agent.h"

/* "&name=value", encoded, onto form */
static void
add(stralloc *form, const char *name, const stralloc *v)
{
  if (!stralloc_cats(form, "&") || !stralloc_cats(form, name) ||
      !stralloc_cats(form, "=") || !url_enc(form, v->s, v->len))
    nomem();
}

/* the string at key in JSON j, NUL-terminated, into out; 1 if found */
static int
str(const stralloc *j, const char *key, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(j->s, j->len, key, &v, &vn) || *v != '"') return 0;
  if (!stralloc_copys(out, "") || !json_unstr(out, v, vn) || !stralloc_0(out))
    nomem();
  --out->len;
  return out->len > 0;
}

/*
 * asks c's token endpoint for tokens, with form (the grant, starting
 * "grant_type=") and kraft's client credentials, and puts what comes
 * back in c.  returns 1, or 0 with the reason in why.
 */
int
mcptokenreq(struct mcpcred *c, stralloc *form, stralloc *why)
{
  static struct http h;
  static stralloc hdrs, body, e, idsecret;
  const char *v;
  size_t vn;
  unsigned long u;

  add(form, "client_id", &c->clientid);
  if (!stralloc_copys(&hdrs, "Content-Type: application/x-www-form-urlencoded\r\n"
      "Accept: application/json\r\n"))
    nomem();
  if (c->secret.len && !str_diff(c->method.s ? c->method.s : "",
      "client_secret_basic")) {
    /* RFC 6749 2.3.1: each part form-encoded, then base64 */
    if (!stralloc_copys(&idsecret, "") ||
        !url_enc(&idsecret, c->clientid.s, c->clientid.len) ||
        !stralloc_cats(&idsecret, ":") ||
        !url_enc(&idsecret, c->secret.s, c->secret.len) ||
        !stralloc_cats(&hdrs, "Authorization: Basic ") ||
        !base64_enc(&hdrs, idsecret.s, idsecret.len) ||
        !stralloc_cats(&hdrs, "\r\n"))
      nomem();
  } else if (c->secret.len)
    add(form, "client_secret", &c->secret);
  if (!stralloc_0(&hdrs)) nomem();

  if (!http_fetch("POST", c->tokenep.s, hdrs.s, form->s, form->len, &h, &body,
      why))
    return 0;
  if (h.status != 200) {
    if (!stralloc_copys(why, "the authorization server said ")) nomem();
    if (str(&body, "error", &e)) {
      if (!stralloc_cat(why, &e)) nomem();
    } else if (!stralloc_cats(why, "HTTP ") ||
        !stralloc_catulong(why, (unsigned long)h.status))
      nomem();
    if (str(&body, "error_description", &e) &&
        (!stralloc_cats(why, ": ") || !stralloc_cat(why, &e)))
      nomem();
    return 0;
  }
  if (!str(&body, "access_token", &c->access)) {
    if (!stralloc_copys(why, "the authorization server sent no access token"))
      nomem();
    return 0;
  }
  str(&body, "refresh_token", &c->refresh);  /* or keep the old one */
  c->expires = 0;
  if (json_get(body.s, body.len, "expires_in", &v, &vn) &&
      scan_ulongb(v, vn, &u) == vn && u)
    c->expires = (unsigned long)time(0) + u;
  return 1;
}
