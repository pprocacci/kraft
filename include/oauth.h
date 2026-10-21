#ifndef OAUTH_H
#define OAUTH_H

#include <stddef.h>
#include "http.h"
#include "stralloc.h"

/*
 * The pieces of OAuth 2.1 that MCP's authorization needs: PKCE,
 * URL encoding both ways, the parameters of a WWW-Authenticate
 * challenge, and a one-off HTTP request.
 */

int oauth_pkce(stralloc *, stralloc *);
int oauth_s256(stralloc *, const char *, size_t);
int url_enc(stralloc *, const char *, size_t);
int url_param(const char *, size_t, const char *, stralloc *);
int www_param(const char *, size_t, const char *, stralloc *);
int http_fetch(const char *, const char *, const char *, const char *,
    size_t, struct http *, stralloc *, stralloc *);

#endif
