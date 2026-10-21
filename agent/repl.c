#include "agent.h"

/* a number of tokens, short: 950, 12.4K, 1.2M */
static void
catk(stralloc *s, unsigned long u)
{
  const char *unit = u >= 1000000 ? "M" : u >= 1000 ? "K" : "";
  unsigned long d = u >= 1000000 ? 1000000 : u >= 1000 ? 1000 : 1;

  if (!stralloc_catulong(s, u / d)) nomem();
  if (d > 1 && u / d < 100 && (!stralloc_append(s, '.') ||
      !stralloc_catulong(s, u % d * 10 / d)))
    nomem();
  if (!stralloc_cats(s, unit)) nomem();
}

/* the line above the prompt: model, mode, context and tokens so far */
static void
statusline(void)
{
  static stralloc s;
  unsigned long t = total.in + total.cread + total.cwrite + total.out;

  if (!stralloc_copys(&s, " ") || !stralloc_cats(&s, model) ||
      !stralloc_cats(&s, " \302\267 ") || !stralloc_cats(&s, modes[mode]))
    nomem();
  if (effort && (!stralloc_cats(&s, " \302\267 effort ") ||
      !stralloc_cats(&s, effort)))
    nomem();
  if (ctxwin) {
    if (!stralloc_cats(&s, " \302\267 context ") ||
        !stralloc_catulong(&s, estimate() * 100 / ctxwin) ||
        !stralloc_cats(&s, "% of "))
      nomem();
    catk(&s, ctxwin);
  }
  if (t) {
    if (!stralloc_cats(&s, " \302\267 ")) nomem();
    catk(&s, t);
    if (!stralloc_cats(&s, " tokens")) nomem();
    if (cost && !stralloc_cats(&s, " \302\267 ")) nomem();
    if (cost) catcost(&s);
  }
  if (!stralloc_0(&s)) nomem();
  styled(DIM, s.s);
  outs("\n");
}

/* the prompt; libedit is told about escapes by \1 around them */
static const char *
prompt(int more)
{
  static stralloc p;
  const char *w = more ? "..." : mode == MODE_AUTO ? "auto" :
      mode == MODE_RULES ? "rules" : mode == MODE_YES ? "yes" : "";

  if (!stralloc_copys(&p, color ? "\001\033[1;36m\001" : "") ||
      !stralloc_cats(&p, w) || !stralloc_cats(&p, more ? "" : ">") ||
      !stralloc_cats(&p, color ? "\001\033[0m\001 " : " ") || !stralloc_0(&p))
    nomem();
  return p.s;
}

/*
 * reads the next task into line; a line ending in \ goes on to the
 * next.  returns as ui_readline does.
 */
static int
readtask(stralloc *line)
{
  static stralloc more;
  int r;

  if (isatty(0)) {
    nl();
    statusline();
    flush();
  }
  r = ui_readline(line, prompt(0));
  while (r > 0 && line->len && line->s[line->len - 1] == '\\') {
    line->s[line->len - 1] = '\n';
    if (ui_readline(&more, prompt(1)) <= 0) break;
    if (!stralloc_cat(line, &more)) nomem();
  }
  if (r > 0) {
    if (!stralloc_0(line)) nomem();
    --line->len;
  }
  last = '\n';
  return r;
}

/* the interactive loop: a prompt, then a command or a task, until EOF */
void
repl(void)
{
  int r;

  if (isatty(0)) {
    styled(BOLD, "kraft " KRAFT_VERSION);
    sgr(DIM);
    outs(": ");
    outs(model);
    outs(" at ");
    outs(base);
    outs(". /help for help; end a line with \\ to go on to the next.");
    sgr(0);
    outs("\n");
  }
  while ((r = readtask(&line))) {
    if (r < 0) {  /* ^C at the prompt: start a fresh line */
      intr = 0;
      outs("\n");
      continue;
    }
    while (line.len && (line.s[line.len - 1] == ' ' ||
        line.s[line.len - 1] == '\t'))
      --line.len;
    if (!line.len) continue;
    if (!stralloc_0(&line)) nomem();
    --line.len;
    if ((r = command(line.s)) < 0) break;
    if (r) continue;
    status = 0;
    task(line.s, line.len);
    nl();
    flush();
  }
  nl();
  flush();
}
