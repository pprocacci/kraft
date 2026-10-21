#include "byte.h"
#include "str.h"
#include "stralloc.h"
#include "url.h"

/*
 * parses http://host[:port][/path] or https://...; the host may be
 * a bracketed IPv6 literal.  returns 1 on success, 0 on bad syntax
 * or out of memory.
 */
int
url_parse(struct url *u, const char *s)
{
  const char *h, *e;
  size_t n;

  if (str_start(s, "http://")) { u->tls = 0; s += 7; }
  else if (str_start(s, "https://")) { u->tls = 1; s += 8; }
  else return 0;

  if (*s == '[') {
    h = ++s;
    while (*s && *s != ']') ++s;
    if (*s != ']') return 0;
    e = s++;
  } else {
    h = s;
    while (*s && *s != ':' && *s != '/') ++s;
    e = s;
  }
  if (e == h) return 0;
  if (!stralloc_copyb(&u->host, h, (size_t)(e - h))) return 0;
  if (!stralloc_0(&u->host)) return 0;

  if (*s == ':') {
    h = ++s;
    while (*s >= '0' && *s <= '9') ++s;
    if (s == h) return 0;
    if (!stralloc_copyb(&u->port, h, (size_t)(s - h))) return 0;
  } else if (!stralloc_copys(&u->port, u->tls ? "443" : "80")) return 0;
  if (!stralloc_0(&u->port)) return 0;

  if (*s && *s != '/') return 0;
  n = str_len(s);
  while (n && s[n - 1] == '/') --n;
  if (!stralloc_copyb(&u->path, s, n)) return 0;
  if (!stralloc_0(&u->path)) return 0;
  return 1;
}
