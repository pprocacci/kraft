/* the OAuth pieces: PKCE, URL encoding, WWW-Authenticate */
#include <stdio.h>
#include <string.h>
#include "oauth.h"
#include "stralloc.h"

static int fails;

static void
is(const char *what, const stralloc *got, const char *want)
{
  if (got->len != strlen(want) || memcmp(got->s, want, got->len)) {
    printf("FAIL %s: got \"%.*s\", want \"%s\"\n", what, (int)got->len,
        got->s ? got->s : "", want);
    ++fails;
  }
}

static void
param(const char *q, const char *name, int found, const char *want)
{
  stralloc v = STRALLOC_ZERO;

  if (url_param(q, strlen(q), name, &v) != found) {
    printf("FAIL url_param %s in %s: found is not %d\n", name, q, found);
    ++fails;
  } else if (found)
    is(name, &v, want);
  stralloc_free(&v);
}

static void
www(const char *h, const char *name, int found, const char *want)
{
  stralloc v = STRALLOC_ZERO;

  if (www_param(h, strlen(h), name, &v) != found) {
    printf("FAIL www_param %s in %s: found is not %d\n", name, h, found);
    ++fails;
  } else if (found)
    is(name, &v, want);
  else if (v.len) {
    printf("FAIL www_param %s in %s: left \"%.*s\"\n", name, h, (int)v.len, v.s);
    ++fails;
  }
  stralloc_free(&v);
}

int
main(void)
{
  stralloc a = STRALLOC_ZERO, b = STRALLOC_ZERO;
  int i;

  /* RFC 7636, appendix B */
  oauth_s256(&a, "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk", 43);
  is("S256", &a, "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
  for (i = 0; i < 20; ++i) {
    oauth_pkce(&a, &b);
    if (a.len != 43 || b.len != 43 || strspn(a.s, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz0123456789-_") < 43) {
      printf("FAIL pkce: %.*s\n", (int)a.len, a.s);
      ++fails;
    }
  }

  stralloc_copys(&a, "");
  url_enc(&a, "http://127.0.0.1:8/cb?a=b c&d=é~", 33);
  is("url_enc", &a, "http%3A%2F%2F127.0.0.1%3A8%2Fcb%3Fa%3Db%20c%26d%3D%C3%A9~");

  param("http://127.0.0.1:5/callback?code=ab%2Bc&state=x+y#frag", "code", 1, "ab+c");
  param("http://127.0.0.1:5/callback?code=ab%2Bc&state=x+y#frag", "state", 1, "x y");
  param("/callback?state=s&code=", "code", 1, "");
  param("/callback?xcode=1", "code", 0, 0);
  param("grant_type=refresh_token&refresh_token=r%3D1", "refresh_token", 1, "r=1");
  param("a=%zz&b=%4", "a", 1, "%zz");
  param("a=%zz&b=%4", "b", 1, "%4");

  www("Bearer resource_metadata=\"https://x/.well-known/oauth-protected-"
      "resource\", scope=\"a b\"", "resource_metadata", 1,
      "https://x/.well-known/oauth-protected-resource");
  www("Bearer resource_metadata=\"https://x/r\", scope=\"a b\"", "scope", 1, "a b");
  www("Bearer realm=example, Scope=files", "scope", 1, "files");
  www("Bearer error=\"invalid_token\", error_description=\"a \\\"b\\\"\"",
      "error_description", 1, "a \"b\"");
  www("Bearer realm=\"x\"", "scope", 0, 0);
  www("Bearer abc==", "scope", 0, 0);
  www("", "scope", 0, 0);

  if (fails) return 1;
  printf("oauth: ok\n");
  return 0;
}
