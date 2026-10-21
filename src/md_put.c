#include "md.h"
#include "stralloc.h"

#define C(x) do { if (!(x)) return 0; } while (0)

/* sets the terminal's attributes to match the state */
static int
style(const struct md *m, stralloc *o)
{
  C(stralloc_cats(o, "\033[0m"));
  if (m->fence || m->fenceline) return stralloc_cats(o, m->fenceline ?
      "\033[2m" : "\033[36m");
  if (m->bold || m->heading) C(stralloc_cats(o, "\033[1m"));
  if (m->code) C(stralloc_cats(o, "\033[36m"));
  return 1;
}

static int
lone(struct md *m, stralloc *o)
{
  /* a * that turned out not to be half of ** */
  if (!m->star) return 1;
  m->star = 0;
  return stralloc_append(o, '*');
}

/*
 * renders the Markdown in s for a terminal, appending it to o: bold
 * for **...** and # headings (the #s dropped), cyan for `code` and
 * ``` blocks, dim for the ``` lines.  the text may arrive in pieces
 * cut anywhere; m carries the state from one to the next.
 */
int
md_put(struct md *m, stralloc *o, const char *s, size_t n)
{
  size_t i;
  char c;

  for (i = 0; i < n; ++i) {
    c = s[i];

    /* backquotes at the beginning of a line: maybe a fence */
    if (m->bol && !m->fenceline && c == '`') {
      if (++m->ticks == 3) {
        m->ticks = 0;
        m->fence = !m->fence;
        m->fenceline = 1;
        C(style(m, o));
        C(stralloc_cats(o, "```"));
      }
      continue;
    }
    if (m->ticks) {
      /* one or two: inline code after all, or just code in a block */
      while (m->ticks) {
        --m->ticks;
        if (!m->fence) {
          m->code = !m->code;
          C(style(m, o));
        }
        C(stralloc_append(o, '`'));
      }
      m->bol = 0;
    }

    if (c == '\n') {
      C(lone(m, o));
      if (m->hashes) {
        while (m->hashes) { --m->hashes; C(stralloc_append(o, '#')); }
      }
      m->heading = 0;
      if (m->fenceline) m->fenceline = 0;
      /* plain at the end of the line; the next line's style after it */
      C(stralloc_cats(o, "\033[0m\n"));
      if (m->fence || m->bold || m->code) C(style(m, o));
      m->bol = 1;
      continue;
    }
    if (m->fenceline || m->fence) {
      C(stralloc_append(o, c));
      m->bol = 0;
      continue;
    }

    /* # heading */
    if (m->bol && c == '#') { ++m->hashes; continue; }
    if (m->hashes) {
      if (c == ' ' && m->hashes <= 6) {
        m->hashes = 0;
        m->heading = 1;
        m->bol = 0;
        C(style(m, o));
        continue;
      }
      while (m->hashes) { --m->hashes; C(stralloc_append(o, '#')); }
    }
    m->bol = 0;

    if (c == '*' && !m->code) {
      if (m->star) {
        m->star = 0;
        m->bold = !m->bold;
        C(style(m, o));
      } else
        m->star = 1;
      continue;
    }
    C(lone(m, o));
    if (c == '`') {
      m->code = !m->code;
      C(style(m, o));
      continue;
    }
    C(stralloc_append(o, c));
  }
  return 1;
}
