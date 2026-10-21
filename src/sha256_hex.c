#include <openssl/evp.h>
#include "cfg.h"
#include "stralloc.h"

/* appends the SHA-256 of s, in lower-case hex, to out */
int
sha256_hex(stralloc *out, const char *s, size_t n)
{
  static const char hex[] = "0123456789abcdef";
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len, i;

  if (!EVP_Digest(s, n, md, &len, EVP_sha256(), 0)) return 0;
  for (i = 0; i < len; ++i)
    if (!stralloc_append(out, hex[md[i] >> 4]) ||
        !stralloc_append(out, hex[md[i] & 15]))
      return 0;
  return 1;
}
