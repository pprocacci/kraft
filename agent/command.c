#include "agent.h"

static void
help(void)
{
  outs("Type a task and press return. Commands:\n"
      "  /clear    forget the conversation\n"
      "  /compact  replace the conversation with a summary, to save space\n"
      "  /effort   show or set how hard the model thinks: low, medium,\n"
      "            high, xhigh, max, or default\n"
      "  /mode     show or set when edits and commands need approval:\n"
      "              /mode ask    always ask\n"
      "              /mode rules  ask unless fixed safety rules allow it\n"
      "              /mode auto   the rules, then a check by the model\n"
      "                           for what the rules cannot settle\n"
      "              /mode yes    never ask\n"
      "  /undo     put back the files the last task changed (again to go\n"
      "            further back); changes made by commands are not undone\n"
      "  /usage    show tokens used, cache hits, and context size\n"
      "  /rules    list the allow and deny rules from the config files\n"
      "  /diff     show the uncommitted changes (in a git repository)\n"
      "  /commit   have the model commit them, with a fitting message\n"
      "  /help     show this\n"
      "  /exit     quit (or press ctrl-d)\n"
      "^C stops the current reply or command.\n");
}

static void
num(const char *a, unsigned long u, const char *b)
{
  if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, u) ||
      !stralloc_0(&tmp))
    nomem();
  outs(a);
  outs(tmp.s);
  outs(b);
}

static void
showusage(void)
{
  unsigned long in = total.in + total.cread + total.cwrite;

  num("requests:      ", requests, "\n");
  num("input tokens:  ", in, "");
  num(" (", total.cread, " read from the cache, ");
  num("", total.cwrite, " written to it)\n");
  if (in) num("cache hits:    ", total.cread * 100 / in, "% of input\n");
  num("output tokens: ", total.out, "\n");
  num("context now:   ", estimate(), " tokens, roughly");
  if (ctxwin) {
    num(", of a window of ", ctxwin, "");
    num(" (", estimate() * 100 / ctxwin, "%)");
  }
  outs("\n");
}

static void
showrules(void)
{
  unsigned long i;

  if (!nrules) {
    outs("(no rules; see the PERMISSIONS section of kraft(1))\n");
    return;
  }
  for (i = 0; i < nrules; ++i) {
    sgr(rules[i].allow ? GREEN : RED);
    outs(rules[i].allow ? "allow " : "deny ");
    outs(rules[i].tool);
    outs(" ");
    outs(rules[i].pat);
    sgr(DIM);
    outs("   (");
    outs(rules[i].from);
    outs(")");
    sgr(0);
    outs("\n");
  }
}

static void
showmode(void)
{
  outs("(permission mode: ");
  outs(modes[mode]);
  outs(")\n");
}

static void
showdiff(void)
{
  stralloc o = STRALLOC_ZERO;

  if (!ingit) { outs("(not in a git repository)\n"); return; }
  /* against HEAD, or, before the first commit, against nothing */
  sh(color ? "git -c color.ui=always status --short; echo; "
      "if git rev-parse -q --verify HEAD >/dev/null; then "
      "git -c color.ui=always --no-pager diff HEAD; else "
      "git -c color.ui=always --no-pager diff; fi" :
      "git status --short; echo; "
      "if git rev-parse -q --verify HEAD >/dev/null; then "
      "git --no-pager diff HEAD; else git --no-pager diff; fi", &o);
  if (!o.len) outs("(no uncommitted changes)\n");
  else {
    raw(o.s, o.len);
    outs("\n");
  }
  stralloc_free(&o);
}

/*
 * carries out line if it is one of the slash commands.  returns 1 if
 * it was, -1 if it was /exit, 0 if it is a task.
 */
int
command(const char *line)
{
  const char *s;

  if (*line != '/') return 0;
  if (!str_diff(line, "/exit") || !str_diff(line, "/quit")) return -1;
  if (!str_diff(line, "/help")) { help(); return 1; }
  if (!str_diff(line, "/clear")) {
    while (conv.n) conv_drop(&conv);
    lastmain = nousage;
    pending = 0;
    newsession();  /* the old one stays saved, for -r */
    outs("(conversation cleared)\n");
    return 1;
  }
  if (!str_diff(line, "/mode")) { showmode(); return 1; }
  if (str_start(line, "/mode ")) {
    if (!pickmode(line + 6))
      outs("(modes are ask, rules, auto and yes)\n");
    else showmode();
    return 1;
  }
  if (!str_diff(line, "/effort") || str_start(line, "/effort ")) {
    if (line[7] && !pickeffort(line + 8))
      outs("(effort is low, medium, high, xhigh, max or default)\n");
    else {
      outs("(effort: ");
      outs(effort ? effort : "the server's default");
      outs(")\n");
    }
    return 1;
  }
  if (!str_diff(line, "/usage")) { showusage(); return 1; }
  if (!str_diff(line, "/rules")) { showrules(); return 1; }
  if (!str_diff(line, "/diff")) { showdiff(); return 1; }
  if (!str_diff(line, "/commit")) {
    if (!ingit) { outs("(not in a git repository)\n"); return 1; }
    status = 0;
    task(commitprompt, str_len(commitprompt));
    nl();
    flush();
    return 1;
  }
  if (!str_diff(line, "/undo")) {
    if (undo()) save();
    else outs("(nothing to undo)\n");
    return 1;
  }
  if (!str_diff(line, "/compact")) {
    intr = 0;
    if (!conv.n) outs("(nothing to compact)\n");
    else if (compact(0)) save();
    return 1;
  }
  /* "/usr/src: why does this fail?" is a task; "/nosuch" a typo */
  for (s = line + 1; *s && *s != ' ' && *s != '/'; ++s) ;
  if (*s != '/') {
    outs("(no such command; /help lists them)\n");
    return 1;
  }
  return 0;
}
