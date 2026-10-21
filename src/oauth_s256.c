#include <openssl/evp.h>
#include <stdlib.h>
#include "byte.h"
#include "media.h"
#include "oauth.h"
#include "stralloc.h"

/*
 * PKCE's S256 (RFC 7636 4.2): the base64url, unpadded, of the SHA-256
 * of s (n bytes), into out.  with s 0, n random bytes instead, which
 * makes a verifier or a state.
 */
int
oauth_s256(stralloc *out, const char *s, size_t n)
{
  unsigned char md[EVP_MAX_MD_SIZE], r[64];
  unsigned int len;
  size_t i;

  if (s) {
    if (!EVP_Digest(s, n, md, &len, EVP_sha256(), 0)) return 0;
  } else {
    if (n > sizeof r) n = sizeof r;
    arc4random_buf(r, n);
    byte_copy(md, n, r);
    len = (unsigned int)n;
  }
  if (!stralloc_copys(out, "") || !base64_enc(out, (const char *)md, len))
    return 0;
  for (i = 0; i < out->len; ++i)
    if (out->s[i] == '+') out->s[i] = '-';
    else if (out->s[i] == '/') out->s[i] = '_';
  while (out->len && out->s[out->len - 1] == '=') --out->len;
  return 1;
}
