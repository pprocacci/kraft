#include "agent.h"

/* does pattern match s, or (for a deny rule) any part of command s? */
int
hits(const char *pat, const char *s, int parts)
{
  stralloc seg = STRALLOC_ZERO;
  size_t i, e, a, b;
  int hit = !fnmatch(pat, s, 0);

  for (i = 0; parts && !hit && s[i]; i = e + (s[e] != 0)) {
    for (e = i; s[e] && !(s[e] == ';' || s[e] == '&' || s[e] == '|' ||
        s[e] == '\n' || s[e] == '(' || s[e] == ')' || s[e] == '`'); ++e) ;
    for (a = i; a < e && (s[a] == ' ' || s[a] == '\t'); ++a) ;
    for (b = e; b > a && (s[b - 1] == ' ' || s[b - 1] == '\t'); --b) ;
    if (a == b) continue;
    if (!stralloc_copyb(&seg, s + a, b - a) || !stralloc_0(&seg)) nomem();
    hit = !fnmatch(pat, seg.s, 0);
  }
  stralloc_free(&seg);
  return hit;
}
