#include "agent.h"

static void
help(void)
{
  outs("Type a task and press return. Commands:\n"
      "  /clear    forget the conversation\n"
      "  /compact  replace the conversation with a summary, to save space\n"
      "  /init     have the model write a KRAFT.md describing the project\n"
      "  /remember [-g] TEXT   add a note to KRAFT.md (-g: to\n"
      "            ~/.kraft/KRAFT.md, for every project)\n"
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
      "  /model    show or change the model\n"
      "  /usage    show tokens used, cache hits, context size, and cost\n"
      "  /rules    list the allow and deny rules from the config files\n"
      "  /mcp      list the MCP servers and their tools; /mcp login NAME\n"
      "            and /mcp logout NAME for one that wants a login\n"
      "  /diff     show the uncommitted changes (in a git repository)\n"
      "  /commit   have the model commit them, with a fitting message\n"
      "  /help     show this\n"
      "  /exit     quit (or press ctrl-d)\n"
      "^C stops the current reply or command.\n"
      "Your own commands, from .kraft/commands/ and ~/.kraft/commands/:\n");
  customs();
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
  if (!stralloc_copys(&tmp, "")) nomem();
  catcost(&tmp);
  if (!stralloc_0(&tmp)) nomem();
  outs("cost:          ");
  if (!*tmp.s) outs("unknown (not a model on the Anthropic API)\n");
  else {
    outs(tmp.s);
    outs(costgap ? " at list prices; some requests could not be priced\n" :
        " at Anthropic's list prices\n");
  }
}

/* /model [name]: show, or change, the model */
static void
setmodel(const char *m)
{
  while (*m == ' ') ++m;
  if (*m) {
    if (!str_diff(checkmodel, model)) checkmodel = sdup(m);  /* it follows */
    model = sdup(m);
    if (!ctxset) {
      /* a new model, a new window */
      ctxwin = 0;
      findwindow();
    }
  }
  outs("(model: ");
  outs(model);
  outs(")\n");
}

static void
showrules(void)
{
  unsigned long i;

  if (!nrules && !nhooks) {
    outs("(no rules; see the PERMISSIONS section of kraft(1))\n");
    return;
  }
  for (i = 0; i < nhooks; ++i) {
    sgr(CYAN);
    outs(hooks[i].when == HOOK_BEFORE ? "hook before " :
        hooks[i].when == HOOK_AFTER ? "hook after " : "hook done");
    if (hooks[i].tool) outs(hooks[i].tool);
    outs(" ");
    sgr(0);
    outs(hooks[i].cmd);
    sgr(DIM);
    outs("   (");
    outs(hooks[i].from);
    outs(")");
    sgr(0);
    outs("\n");
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

/* /mcp login NAME, /mcp logout NAME */
static void
mcpcmd(const char *line)
{
  static stralloc why;
  const char *name = line + (str_start(line, "/mcp login ") ? 11 : 12);
  char was[sizeof tools / sizeof tools[0]][80];
  unsigned long k;
  size_t i, j;

  while (*name == ' ') ++name;
  for (k = 0; k < nmcps && str_diff(mcps[k].name, name); ++k) ;
  if (k == nmcps) {
    kwarn("there is no MCP server called ", name, " (/mcp lists them)");
    return;
  }
  if (line[5] == 'l' && line[8] == 'o' && line[9] == 'u') {  /* logout */
    mcplogout(k);
    return;
  }
  if (!mcplogin(k)) return;
  if (!stralloc_copys(&why, "")) nomem();
  /* its tools may be new: the list changes, the answers "a" stay */
  for (i = 0; tools[i].name; ++i) {
    was[i][0] = 0;
    if (allowed[i] && str_len(tools[i].name) < sizeof was[i])
      byte_copy(was[i], str_len(tools[i].name) + 1, tools[i].name);
  }
  was[i][0] = 1;  /* the end */
  if (!mcpup(k, &why)) {
    if (!stralloc_0(&why)) nomem();
    kwarn("MCP server ", mcps[k].name, " did not start");
    outs("  (");
    outs(why.s);
    outs(")\n");
  }
  settools();
  for (i = 0; tools[i].name; ++i) {
    allowed[i] = 0;
    for (j = 0; was[j][0] != 1; ++j)
      if (was[j][0] && !str_diff(was[j], tools[i].name)) allowed[i] = 1;
  }
  mcplist();
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

static const char initprompt[] =
    "Write a KRAFT.md for this project, in its top directory, for a coding "
    "agent working on it later. Look around first: the README and other "
    "documents, the build files, the layout of the source, the tests, and "
    "the style of the code and of the commit messages. Then write down, "
    "briefly, what is not obvious from a quick look: how to build, test and "
    "run it; how the code is organised; the conventions it follows; and "
    "anything that is easy to get wrong. Keep it short, a page or so, in "
    "plain Markdown. If there is a KRAFT.md already, improve it rather than "
    "starting again.";

/*
 * /remember [-g] text: a line for the model to keep, added to the
 * project's KRAFT.md (or, with -g, ~/.kraft/KRAFT.md), and told to the
 * model now, since the system prompt only changes in the next session.
 */
static void
remember(const char *s)
{
  stralloc fn = STRALLOC_ZERO, l = STRALLOC_ZERO;
  const char *home;
  struct msg *m;
  struct block *b;
  int fd, global = 0;

  while (*s == ' ') ++s;
  if (str_start(s, "-g ")) {
    global = 1;
    for (s += 3; *s == ' '; ++s) ;
  }
  if (!*s) {
    outs("(usage: /remember [-g] something to keep in mind)\n");
    return;
  }
  if (global) {
    if (!(home = env("HOME", 0)) || !sessdir(&fn)) return;  /* ~/.kraft */
    fn.len -= str_len("/sessions");
    if (!stralloc_cats(&fn, "/KRAFT.md")) nomem();
  } else if (!stralloc_copys(&fn, "KRAFT.md")) nomem();
  if (!stralloc_0(&fn) || !stralloc_copys(&l, "- ") || !stralloc_cats(&l, s) ||
      !stralloc_cats(&l, "\n"))
    nomem();
  fd = open(fn.s, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
  if (fd == -1 || buffer_unixwrite(fd, l.s, l.len) != (ssize_t)l.len) {
    kwarnsys("unable to add to ", fn.s);
    if (fd != -1) close(fd);
    goto out;
  }
  close(fd);
  if (!(m = conv_msg(&conv, ROLE_USER)) || !(b = conv_block(m, BLK_TEXT)))
    nomem();
  if (!stralloc_copys(&b->text, "[The user asks you to keep this in mind from "
      "now on; it has been added to ") || !stralloc_cats(&b->text, fn.s) ||
      !stralloc_cats(&b->text, ": ") || !stralloc_cats(&b->text, s) ||
      !stralloc_cats(&b->text, "]"))
    nomem();
  pending += b->text.len;
  save();
  sgr(DIM);
  outs("(added to ");
  outs(fn.s);
  outs(")");
  sgr(0);
  outs("\n");
out:
  stralloc_free(&fn);
  stralloc_free(&l);
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
  if (!str_diff(line, "/model") || str_start(line, "/model ")) {
    setmodel(line + 6);
    return 1;
  }
  if (!str_diff(line, "/rules")) { showrules(); return 1; }
  if (!str_diff(line, "/mcp")) { mcplist(); return 1; }
  if (str_start(line, "/mcp login ") || str_start(line, "/mcp logout ")) {
    mcpcmd(line);
    return 1;
  }
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
  if (!str_diff(line, "/init")) {
    status = 0;
    task(initprompt, str_len(initprompt));
    nl();
    flush();
    return 1;
  }
  if (!str_diff(line, "/remember") || str_start(line, "/remember ")) {
    remember(line + 9);
    return 1;
  }
  if (!str_diff(line, "/compact")) {
    intr = 0;
    if (!conv.n) outs("(nothing to compact)\n");
    else if (compact(0)) save();
    return 1;
  }
  if (mcpprompt(line) || custom(line)) return 1;
  /* "/usr/src: why does this fail?" is a task; "/nosuch" a typo */
  for (s = line + 1; *s && *s != ' ' && *s != '/'; ++s) ;
  if (*s != '/') {
    outs("(no such command; /help lists them)\n");
    return 1;
  }
  return 0;
}
