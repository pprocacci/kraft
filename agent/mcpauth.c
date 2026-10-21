#include "agent.h"

/*
 * gives server k the access token from its saved login, refreshing it
 * first if it has run out, or will within a minute, or (renew) the
 * server has just turned it down.  returns 1 if it has a token now.
 */
int
mcpauth(unsigned long k, int renew)
{
  static struct mcpcred c;
  static stralloc form, why;
  struct mcp *m = &mcps[k];

  m->token.len = 0;
  if (!m->http || !mcpcredload(k, &c) || !c.access.len) return 0;
  if (!renew && (!c.expires || c.expires > (unsigned long)time(0) + 60)) {
    if (!stralloc_copy(&m->token, &c.access)) nomem();
    return 1;
  }
  if (!c.refresh.len || !c.tokenep.len) return 0;
  if (!stralloc_copys(&form, "grant_type=refresh_token&refresh_token=") ||
      !url_enc(&form, c.refresh.s, c.refresh.len) ||
      !stralloc_cats(&form, "&resource=") ||
      !url_enc(&form, c.url.s, c.url.len))
    nomem();
  if (!stralloc_copys(&why, "")) nomem();
  if (!mcptokenreq(&c, &form, &why)) {
    if (!stralloc_0(&why)) nomem();
    kwarn("MCP server ", m->name, ": the login could not be renewed");
    sgr(DIM);
    outs("  (");
    outs(why.s);
    outs("; /mcp login to log in again)\n");
    sgr(0);
    return 0;
  }
  mcpcredsave(k, &c);
  if (!stralloc_copy(&m->token, &c.access)) nomem();
  return 1;
}
