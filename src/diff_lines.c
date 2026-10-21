#include <errno.h>
#include <stdint.h>
#include "alloc.h"
#include "byte.h"
#include "diff.h"
#include "stralloc.h"

#define MAXCELLS 4000000  /* the largest table for the exact diff */

struct lines {
  size_t *at;   /* where each line starts */
  size_t n;     /* how many */
  const char *s;
  size_t len;
};

static int
split(struct lines *l, const char *s, size_t len)
{
  size_t i, k = 0;

  l->s = s;
  l->len = len;
  l->n = 0;
  for (i = 0; i < len; ++i)
    if (s[i] == '\n') ++k;
  if (len && s[len - 1] != '\n') ++k;
  if (!(l->at = alloc((k + 1) * sizeof *l->at))) return 0;
  for (i = 0; i < len; i = byte_chr(s + i, len - i, '\n') + i + 1)
    l->at[l->n++] = i;
  l->at[l->n] = len;
  return 1;
}

/* line i, without its newline */
static size_t
linelen(const struct lines *l, size_t i)
{
  size_t e = l->at[i + 1];

  if (e > l->at[i] && l->s[e - 1] == '\n') --e;
  return e - l->at[i];
}

static int
same(const struct lines *a, size_t i, const struct lines *b, size_t j)
{
  size_t n = linelen(a, i);

  return n == linelen(b, j) && !byte_diff(a->s + a->at[i], n, b->s + b->at[j]);
}

static int
put(stralloc *out, char op, const struct lines *l, size_t i)
{
  return stralloc_append(out, op) &&
      stralloc_catb(out, l->s + l->at[i], linelen(l, i)) &&
      stralloc_append(out, '\n');
}

/*
 * appends a line diff turning a into b to out, as lines that start
 * with ' ' (the same in both), '-' (only in a) or '+' (only in b),
 * with at most ctx lines of the unchanged ones on either side of a
 * change, and "@@ N" before each run of them, N being the number in
 * b of its first line.  if the two are too big to compare exactly,
 * the differing middle is shown as all removed, then all added.
 * returns 1, or 0 if memory runs out.
 */
int
diff_lines(stralloc *out, const char *as, size_t an, const char *bs,
    size_t bn, unsigned long ctx)
{
  struct lines a = { 0, 0, 0, 0 }, b = { 0, 0, 0, 0 };
  unsigned int *t = 0;
  char *ops = 0;          /* the edit script: ' ', '-', '+' */
  char *show = 0;         /* which of it to show */
  size_t *ai = 0, *bj = 0;
  size_t pre, suf, m, n, i, j, k, nops = 0, last, h;
  int ok = 0;

  if (!split(&a, as, an) || !split(&b, bs, bn)) goto out;
  for (pre = 0; pre < a.n && pre < b.n && same(&a, pre, &b, pre); ++pre) ;
  for (suf = 0; suf < a.n - pre && suf < b.n - pre &&
      same(&a, a.n - 1 - suf, &b, b.n - 1 - suf); ++suf) ;
  m = a.n - pre - suf;
  n = b.n - pre - suf;

  k = a.n + b.n + 1;
  if (!(ops = alloc(k)) || !(ai = alloc(k * sizeof *ai)) ||
      !(bj = alloc(k * sizeof *bj)))
    goto out;
#define OP(c, x, y) do { ops[nops] = (c); ai[nops] = (x); bj[nops] = (y); \
    ++nops; } while (0)
  for (i = 0; i < pre; ++i) OP(' ', i, i);
  if (m && n && (m + 1) * (n + 1) <= MAXCELLS) {
    /* the longest common subsequence of the middle, from the end */
    if (!(t = alloc((m + 1) * (n + 1) * sizeof *t))) goto out;
#define T(x, y) t[(x) * (n + 1) + (y)]
    for (i = m + 1; i-- > 0;)
      for (j = n + 1; j-- > 0;) {
        if (i == m || j == n) T(i, j) = 0;
        else if (same(&a, pre + i, &b, pre + j)) T(i, j) = T(i + 1, j + 1) + 1;
        else T(i, j) = T(i + 1, j) > T(i, j + 1) ? T(i + 1, j) : T(i, j + 1);
      }
    for (i = j = 0; i < m || j < n;) {
      if (i < m && j < n && same(&a, pre + i, &b, pre + j)) {
        OP(' ', pre + i, pre + j); ++i; ++j;
      } else if (i < m && (j == n || T(i + 1, j) >= T(i, j + 1))) {
        OP('-', pre + i, 0); ++i;  /* removals before additions */
      } else {
        OP('+', 0, pre + j); ++j;
      }
    }
  } else {
    for (i = 0; i < m; ++i) OP('-', pre + i, 0);
    for (j = 0; j < n; ++j) OP('+', 0, pre + j);
  }
  for (i = 0; i < suf; ++i) OP(' ', a.n - suf + i, b.n - suf + i);

  /*
   * the changes, with ctx lines of what is around them: a line is
   * shown if it changed or a change is within ctx lines of it
   */
  if (!(show = alloc(nops + 1))) goto out;
  for (k = 0, last = (size_t)-1; k < nops; ++k) {
    if (ops[k] != ' ') last = k;
    show[k] = ops[k] != ' ' || (last != (size_t)-1 && k - last <= ctx);
  }
  for (k = nops, last = (size_t)-1; k-- > 0;) {
    if (ops[k] != ' ') last = k;
    if (last != (size_t)-1 && last - k <= ctx) show[k] = 1;
  }
  for (k = 0, h = 0; k < nops; ++k) {
    if (show[k]) {
      if (!k || !show[k - 1]) {
        /* a hunk starts: say where, in the new file */
        if (!stralloc_cats(out, "@@ ") ||
            !stralloc_catulong(out, (unsigned long)h + 1) ||
            !stralloc_append(out, '\n'))
          goto out;
      }
      if (!put(out, ops[k], ops[k] == '+' ? &b : &a,
          ops[k] == '+' ? bj[k] : ai[k]))
        goto out;
    }
    if (ops[k] != '-') ++h;  /* lines of the new file so far */
  }
  ok = 1;
out:
  alloc_free(a.at);
  alloc_free(b.at);
  alloc_free(t);
  alloc_free(ops);
  alloc_free(ai);
  alloc_free(bj);
  alloc_free(show);
  return ok;
}
