#include "oauth.h"
#include "stralloc.h"

/*
 * a new PKCE pair: verifier, 43 characters from 32 random bytes, and
 * challenge, its S256.  challenge may be 0 for just a random string,
 * as for the state parameter.
 */
int
oauth_pkce(stralloc *verifier, stralloc *challenge)
{
  if (!oauth_s256(verifier, 0, 32)) return 0;
  return !challenge || oauth_s256(challenge, verifier->s, verifier->len);
}
