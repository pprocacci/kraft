#include "agent.h"

/* shows up to max lines of s, each with prefix p */
void
lines(const char *p, const char *s, size_t n, unsigned long max)
{
  unsigned long count = 0, more = 0;
  const char *code, *q;
  size_t i, e;

  /* the prefix says what the lines are */
  for (q = p + str_len(p); q > p && (q[-1] == ' ' || q[-1] == '\t'); --q) ;
  switch (q > p ? q[-1] : 0) {
  case '|': code = DIM; break;
  case '!': case '-': code = RED; break;
  case '+': code = GREEN; break;
  default: code = BOLD;
  }
  for (i = 0; i < n; i = e + 1) {
    e = i + byte_chr(s + i, n - i, '\n');
    if (count == max) { ++more; continue; }
    ++count;
    sgr(code);
    outs(p);
    out(s + i, e - i > 200 ? 200 : e - i);
    if (e - i > 200) outs("...");
    sgr(0);
    outs("\n");
  }
  if (more) {
    sgr(DIM);
    outs(p);
    outs("... (");
    if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, more) ||
        !stralloc_0(&tmp))
      nomem();
    outs(tmp.s);
    outs(more == 1 ? " more line)" : " more lines)");
    sgr(0);
    outs("\n");
  }
}
