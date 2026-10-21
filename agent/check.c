#include "agent.h"

/*
 * the auto-mode check: asks the model whether a call the rules could
 * not settle is safe.  the model sees the user's recent requests and
 * the call, never tool output, so files it read cannot argue for it.
 * returns 1 if it says ALLOW; either way its reason is in cwhy.
 */
int
check(const struct tool *t, const char *in, size_t n)
{
  const struct block *req[3];
  struct block *b;
  struct msg *m;
  char cwd[PATH_MAX];
  size_t i, j, k, count = 0;
  int r, allow = 0, ask;

  while (cconv.n) conv_drop(&cconv);
  if (!(m = conv_msg(&cconv, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
    nomem();

  /* the user's last few requests, oldest first */
  for (i = conv.n; i-- > 0 && count < 3;) {
    if (conv.m[i].role != ROLE_USER) continue;
    for (j = conv.m[i].n; j-- > 0 && count < 3;)
      if (conv.m[i].b[j].type == BLK_TEXT) req[count++] = &conv.m[i].b[j];
  }
  if (!stralloc_copys(&b->text, "Working directory: ") ||
      !stralloc_cats(&b->text, getcwd(cwd, sizeof cwd) ? cwd : "unknown") ||
      !stralloc_cats(&b->text, "\n\nWhat the user asked for, most recent "
      "last:\n"))
    nomem();
  for (k = count; k-- > 0;) {
    if (!stralloc_cats(&b->text, "<request>\n") ||
        !stralloc_catb(&b->text, req[k]->text.s,
        req[k]->text.len > 4000 ? 4000 : req[k]->text.len) ||
        !stralloc_cats(&b->text, "\n</request>\n"))
      nomem();
  }
  if (!stralloc_cats(&b->text, "\nThe action the agent wants to take:\n"
      "<action tool=\"") || !stralloc_cats(&b->text, t->name) ||
      !stralloc_cats(&b->text, "\">\n") ||
      !stralloc_catb(&b->text, in, n > 8000 ? 8000 : n) ||
      !stralloc_cats(&b->text, "\n</action>\n\nThe automatic rules could "
      "not settle it: ") || !stralloc_cats(&b->text, rwhy.s) ||
      !stralloc_cats(&b->text, "."))
    nomem();

  sgr(DIM);
  outs("  (auto mode: checking with ");
  outs(checkmodel);
  outs(")");
  sgr(0);
  outs("\n");
  flush();
  effortnow = "low";  /* a short judgement needs little thought */
  r = exchange(&cconv, checksys, notools, checkmodel, 4096, 0);
  effortnow = 0;
  if (!stralloc_copys(&cwhy, "")) nomem();
  if (r > 0) {
    m = &cconv.m[cconv.n - 1];
    for (i = 0; i < m->n; ++i)
      if (m->b[i].type == BLK_TEXT &&
          !stralloc_catb(&cwhy, m->b[i].text.s, m->b[i].text.len))
        nomem();
  }
  /* the verdict, then the reason: first line after it, trimmed */
  for (i = 0; i < cwhy.len && (cwhy.s[i] == ' ' || cwhy.s[i] == '\n' ||
      cwhy.s[i] == '*' || cwhy.s[i] == '#'); ++i) ;
  allow = cwhy.len - i >= 5 && !byte_diff(cwhy.s + i, 5, "ALLOW") &&
      (cwhy.len - i == 5 || !((cwhy.s[i + 5] >= 'A' && cwhy.s[i + 5] <= 'Z') ||
      (cwhy.s[i + 5] >= 'a' && cwhy.s[i + 5] <= 'z')));
  ask = !allow && cwhy.len - i >= 3 && !byte_diff(cwhy.s + i, 3, "ASK");
  while (i < cwhy.len && cwhy.s[i] != ' ' && cwhy.s[i] != '\n' &&
      cwhy.s[i] != ':')
    ++i;
  for (;;) {
    if (i < cwhy.len && (cwhy.s[i] == ' ' || cwhy.s[i] == '\n' ||
        cwhy.s[i] == ':' || cwhy.s[i] == '*' || cwhy.s[i] == '-' ||
        cwhy.s[i] == '.'))
      ++i;
    else if (cwhy.len - i >= 3 && (!byte_diff(cwhy.s + i, 3, "\342\200\224") ||
        !byte_diff(cwhy.s + i, 3, "\342\200\223")))
      i += 3;  /* em or en dash */
    else
      break;
  }
  for (j = i; j < cwhy.len && cwhy.s[j] != '\n' && j - i < 200; ++j) ;
  if (r <= 0) {
    if (!stralloc_copys(&cwhy, intr ? "interrupted" : "the check failed"))
      nomem();
  } else if (j == i) {
    if (!stralloc_copys(&cwhy, allow ? "it judged the call safe" :
        ask ? "it judged the call risky" : "no clear answer"))
      nomem();
  } else {
    byte_copy(cwhy.s, j - i, cwhy.s + i);
    cwhy.len = j - i;
  }
  if (!stralloc_0(&cwhy)) nomem();
  while (cconv.n) conv_drop(&cconv);
  return r > 0 && allow;
}
