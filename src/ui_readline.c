#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <histedit.h>
#include "buffer.h"
#include "stralloc.h"
#include "ui.h"

/* libedit wants stdio streams; this is the one place they are used */

static EditLine *el;
static History *hist;
static HistEvent ev;
static const char *prompt;
static int tty = -1;

static char *
getprompt(EditLine *e)
{
  (void)e;
  return (char *)prompt;
}

static int
plain(stralloc *line)
{
  char c;
  ssize_t r;

  if (!stralloc_copys(line, "")) return 0;
  for (;;) {
    r = buffer_unixread(0, &c, 1);
    if (r <= 0) return line->len ? 1 : 0;
    if (c == '\n') return 1;
    if (!stralloc_append(line, c)) return 0;
  }
}

/*
 * reads one line of input, without its newline and NUL-terminated
 * (the NUL not counted).  uses libedit, with history, when stdin is
 * a terminal.  returns 1 for a line, 0 at end of input, -1 if the
 * user interrupted the line with ^C.
 */
int
ui_readline(stralloc *line, const char *p)
{
  const char *s;
  int n;

  prompt = p;
  if (tty == -1) tty = isatty(0);
  if (!tty) {
    if (!plain(line)) return 0;
  } else {
    if (!el) {
      el = el_init("kraft", stdin, stdout, stderr);
      hist = history_init();
      history(hist, &ev, H_SETSIZE, 1000);
      el_set(el, EL_HIST, history, hist);
      /* the prompt may hold escapes between \1s, which take no room */
      el_set(el, EL_PROMPT_ESC, getprompt, '\1');
      el_set(el, EL_EDITOR, "emacs");
      el_set(el, EL_SIGNAL, 1);
      el_source(el, 0);
    }
    buffer_flush(buffer_1);
    errno = 0;
    s = el_gets(el, &n);
    if (!s || n <= 0) {
      if (errno == EINTR) {
        el_reset(el);
        return -1;
      }
      return 0;
    }
    if (!stralloc_copyb(line, s, (size_t)n)) return 0;
    while (line->len && (line->s[line->len - 1] == '\n' ||
        line->s[line->len - 1] == '\r'))
      --line->len;
    if (line->len) history(hist, &ev, H_ENTER, s);
  }
  if (!stralloc_0(line)) return 0;
  --line->len;
  return 1;
}
