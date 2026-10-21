#include "agent/agent.h"

static void onint(int sig);
static void forget(const char *name);
static const char * readkey(void);
static void showdiff(void);
static int getjson(const char *p, stralloc *out);
static void findwindow(void);
static void newsession(void);
static void help(void);
static void num(const char *a, unsigned long u, const char *b);
static void showusage(void);
static void catk(stralloc *s, unsigned long u);
static void statusline(void);
static const char * prompt(int more);
static int readtask(stralloc *line);
static void showrules(void);
static void showmode(void);

/*
 * ^C: stop the reply by shutting its connection down, or stop the
 * running command (a second ^C kills it outright).  only
 * async-signal-safe calls here.
 */
static void
onint(int sig)
{
  int e = errno;

  (void)sig;
  if (intr < 2) ++intr;
  if (curfd != -1) shutdown(curfd, SHUT_RDWR);
  if (proc_pid > 0) kill(-proc_pid, intr > 1 ? SIGKILL : SIGINT);
  errno = e;
}

/*
 * removes a variable from the environment that commands inherit, and
 * blanks its value where it came in: unsetenv() alone leaves the
 * original strings, which procstat -e and ps e show any process of
 * the same user
 */
static void
forget(const char *name)
{
  char *v = getenv(name);

  if (v) memset(v, 0, strlen(v));
  unsetenv(name);
}

/*
 * the API key: from KRAFT_API_KEY, the API's own variable, or the
 * file ~/.kraft/NAME.key.  the file may hold the bare key or a shell
 * assignment such as export ANTHROPIC_API_KEY=...; it is never run.
 */
static const char *
readkey(void)
{
  static stralloc key;
  const char *s, *home;
  size_t i, j;

  if ((s = env("KRAFT_API_KEY", env(api->keyenv, 0)))) return s;
  if (!(home = env("HOME", 0))) return 0;
  if (!stralloc_copys(&tmp, home) || !stralloc_cats(&tmp, "/.kraft/") ||
      !stralloc_cats(&tmp, api->name) || !stralloc_cats(&tmp, ".key") ||
      !stralloc_0(&tmp))
    nomem();
  if (!file_get(tmp.s, &key)) {
    if (errno != ENOENT) kwarnsys("unable to read ", tmp.s);
    return 0;
  }
  i = byte_chr(key.s, key.len, '=');
  i = i < key.len ? i + 1 : 0;
  j = key.len;
  while (i < j && (key.s[i] == ' ' || key.s[i] == '"' || key.s[i] == '\''))
    ++i;
  while (j > i && (key.s[j - 1] == ' ' || key.s[j - 1] == '\n' ||
      key.s[j - 1] == '\r' || key.s[j - 1] == '"' || key.s[j - 1] == '\''))
    --j;
  if (i == j) return 0;
  byte_copy(key.s, j - i, key.s + i);
  key.len = j - i;
  if (!stralloc_0(&key)) nomem();
  return key.s;
}

static void
showdiff(void)
{
  stralloc o = STRALLOC_ZERO;

  if (!ingit) { outs("(not in a git repository)\n"); return; }
  sh(color ? "git -c color.ui=always status --short; echo; "
      "git -c color.ui=always --no-pager diff HEAD" :
      "git status --short; echo; git --no-pager diff HEAD", &o);
  if (!o.len) outs("(no uncommitted changes)\n");
  else {
    raw(o.s, o.len);
    outs("\n");
  }
  stralloc_free(&o);
}

/* fetches p from the server into out; 1 if it answered 200 */
static int
getjson(const char *p, stralloc *out)
{
  char buf[4096];
  ssize_t n = -1;
  int fd;

  fd = tcp_connect(url.host.s, url.port.s, CONNECTSECS);
  if (fd == -1) return 0;
  if (url.tls && (fd = tls_connect(fd, url.host.s, &tlswhy)) == -1) return 0;
  curfd = fd;
  if (http_request(fd, "GET", host.s, p, ghdrs.s, 0, 0) &&
      http_response(&http, fd) > 0 && http.status == 200) {
    if (!stralloc_copys(out, "")) nomem();
    while ((n = http_get(&http, buf, sizeof buf)) > 0)
      if (out->len < 1048576 && !stralloc_catb(out, buf, (size_t)n)) nomem();
  }
  curfd = -1;
  close(fd);
  return n == 0;
}

/*
 * asks the server for the model's context window: the Anthropic
 * Models API's max_input_tokens, or LM Studio's loaded context.
 * leaves ctxwin 0 if neither answers.
 */
static void
findwindow(void)
{
  static const char *const where[][2] = {
    { "/v1/models/", "max_input_tokens" },
    { "/api/v0/models/", "loaded_context_length" },
    { "/api/v0/models/", "max_context_length" },
  };
  stralloc p = STRALLOC_ZERO, j = STRALLOC_ZERO;
  const char *v;
  size_t i, vn;
  unsigned long u;

  for (i = 0; i < sizeof where / sizeof where[0] && !ctxwin && !intr; ++i) {
    if (!stralloc_copy(&p, &pfx) || !stralloc_cats(&p, where[i][0]) ||
        !stralloc_cats(&p, model) || !stralloc_0(&p))
      nomem();
    if (!getjson(p.s, &j)) continue;
    if (json_get(j.s, j.len, where[i][1], &v, &vn) &&
        scan_ulong(v, &u) == vn && u)
      ctxwin = u;
  }
  stralloc_free(&p);
  stralloc_free(&j);
}

/* picks a new session file; nothing is written until there is a message */
static void
newsession(void)
{
  char stamp[32];
  time_t now = time(0);

  if (!stralloc_copys(&sessfn, "")) nomem();
  if (!sessdir(&sessfn)) return;
  if (!strftime(stamp, sizeof stamp, "/%Y%m%d-%H%M%S-", localtime(&now)))
    stamp[0] = 0;
  if (!stralloc_cats(&sessfn, stamp) ||
      !stralloc_catulong(&sessfn, (unsigned long)getpid()) ||
      !stralloc_cats(&sessfn, ".json") || !stralloc_0(&sessfn))
    nomem();
  setundo();
}

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

int
main(int argc, char **argv)
{
  const char *aname, *key;
  int opt, resumeit = 0;
  char cwd[PATH_MAX];
  size_t i;
  struct sigaction sa;
  int r;

  signal(SIGPIPE, SIG_IGN);
  /* colour for a terminal, unless NO_COLOR (no-color.org) says not */
  color = isatty(1) && !getenv("NO_COLOR") && env("TERM", 0) &&
      str_diff(env("TERM", ""), "dumb");
  sa.sa_handler = onint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  /* no SA_RESTART: blocked calls must return EINTR */
  sigaction(SIGINT, &sa, 0);
  atexit(job_killall);  /* background commands do not outlive kraft */

  /* defaults, then the config files, then the environment, then flags */
  loadconfigs();
  aname = env("KRAFT_API", cfgapi ? cfgapi : "anthropic");
  model = env("KRAFT_MODEL", model);
  base = env("KRAFT_URL", base ? base : "http://127.0.0.1:1234");
  checkmodel = env("KRAFT_CHECK_MODEL", checkmodel);
  if (!pickmode(env("KRAFT_MODE", modes[mode])))
    strerr_die(100, FATAL, "KRAFT_MODE must be ask, rules, auto or yes", 0);
  if ((key = env("KRAFT_EFFORT", 0)) && !pickeffort(key))
    strerr_die(100, FATAL, "KRAFT_EFFORT must be low, medium, high, xhigh, "
        "max or default", 0);
  if ((key = env("KRAFT_CONTEXT", 0)) &&
      (scan_ulong(key, &ctxwin) != str_len(key) || !ctxwin))
    strerr_die(100, FATAL, "KRAFT_CONTEXT must be a number of tokens", 0);
  while ((opt = getopt(argc, argv, "a:c:e:m:n:P:rtu:wy")) != -1)
    switch (opt) {
    case 'a': aname = optarg; break;
    case 'c':
      if (!*optarg || scan_ulong(optarg, &ctxwin) != str_len(optarg) ||
          !ctxwin)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 'e': if (!pickeffort(optarg)) strerr_die(100, USAGE, 0, 0); break;
    case 'm': model = optarg; break;
    case 'n':
      if (!*optarg || scan_ulong(optarg, &max) != str_len(optarg) || !max)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 'P': if (!pickmode(optarg)) strerr_die(100, USAGE, 0, 0); break;
    case 'r': resumeit = 1; break;
    case 'w': websearch = 1; break;
    case 't': showthink = 1; break;
    case 'u': base = optarg; break;
    case 'y': mode = MODE_YES; break;
    default: strerr_die(100, USAGE, 0, 0);
    }
  argv += optind;

  for (i = 0; i < sizeof apis / sizeof apis[0]; ++i)
    if (!str_diff(aname, apis[i].name)) api = &apis[i];
  if (!api) strerr_die(100, FATAL, "unknown API: ", aname);
  if (!model) strerr_die(100, FATAL, "no model: use -m or set KRAFT_MODEL", 0);
  if (!checkmodel) checkmodel = model;
  if (!url_parse(&url, base)) strerr_die(100, FATAL, "bad URL: ", base);

  /* accept base URLs given with or without a trailing /v1 */
  i = url.path.len - 1;
  if (i >= 3 && !byte_diff(url.path.s + i - 3, 3, "/v1")) i -= 3;
  if (!stralloc_copyb(&pfx, url.path.s, i)) nomem();
  if (!stralloc_copy(&path, &pfx)) nomem();
  if (!stralloc_cats(&path, api->endpoint)) nomem();
  if (!stralloc_0(&path)) nomem();

  if (byte_chr(url.host.s, url.host.len, ':') < url.host.len - 1) {
    if (!stralloc_copys(&host, "[") || !stralloc_cats(&host, url.host.s) ||
        !stralloc_cats(&host, "]"))
      nomem();
  } else if (!stralloc_copys(&host, url.host.s)) nomem();
  if (str_diff(url.port.s, url.tls ? "443" : "80")) {
    if (!stralloc_cats(&host, ":") || !stralloc_cats(&host, url.port.s))
      nomem();
  }
  if (!stralloc_0(&host)) nomem();

  if (!stralloc_copys(&hdrs, "Content-Type: application/json\r\n"
      "Accept: text/event-stream\r\n") || !stralloc_cats(&hdrs, api->hdrs))
    nomem();
  if (!stralloc_copys(&ghdrs, api->hdrs)) nomem();
  if ((key = readkey())) {
    if (!stralloc_cats(&hdrs, api->keyhdr) || !stralloc_cats(&hdrs, key) ||
        !stralloc_cats(&hdrs, "\r\n"))
      nomem();
    if (!stralloc_cats(&ghdrs, api->keyhdr) || !stralloc_cats(&ghdrs, key) ||
        !stralloc_cats(&ghdrs, "\r\n"))
      nomem();
  }
  if (!stralloc_0(&hdrs) || !stralloc_0(&ghdrs)) nomem();
  /* the key is in the headers now; keep it from the model's commands */
  forget("KRAFT_API_KEY");
  forget("ANTHROPIC_API_KEY");
  forget("ANTHROPIC_AUTH_TOKEN");
  forget("OPENAI_API_KEY");
  mksystem();
  /* the tools for the whole session: changing them would spoil the cache */
  for (i = 0; tool_all[i].name && i < sizeof tools / sizeof tools[0] - 3; ++i)
    tools[i] = tool_all[i];
  tools[i++] = tasktool;
  pdfok = api->render == anth_render && !str_diff(url.host.s, "api.anthropic.com");
  if (websearch) {
    if (pdfok)
      tools[i++] = searchtool;
    else
      kwarn("-w needs the Anthropic API at api.anthropic.com; ignored", 0, 0);
  }
  tools[i].name = 0;
  /* a sub-agent's: no sub-agents of its own, no user to ask, no checklist */
  for (i = r = 0; tools[i].name; ++i)
    if (str_diff(tools[i].name, "task") && str_diff(tools[i].name, "ask_user") &&
        str_diff(tools[i].name, "todo"))
      subtools[r++] = tools[i];
  subtools[r].name = 0;
  if (!stralloc_copy(&subsys, &sys)) nomem();
  subsys.len = str_len(subsys.s);
  if (!stralloc_cats(&subsys, "\n\nYou are working as a sub-agent: the main "
      "agent has given you one task. Work on it with the tools, then reply "
      "with a concise but complete report of what you found or did. The "
      "report is all the main agent will see, so include the file names, "
      "line numbers and facts it will need. You cannot ask the user "
      "anything; if the brief is unclear, make sensible assumptions and "
      "say what they were.") || !stralloc_0(&subsys))
    nomem();
  if (!ctxwin) findwindow();
  if (!stralloc_copys(&cwdsa, getcwd(cwd, sizeof cwd) ? cwd : ".") ||
      !stralloc_0(&cwdsa))
    nomem();
  if (resumeit && !resume()) outs("(no earlier session in this directory)\n");
  if (!sessfn.len) newsession();

  /* a task on the command line: do it and exit */
  if (*argv) {
    if (!stralloc_copys(&line, "")) nomem();
    for (; *argv; ++argv) {
      if (line.len && !stralloc_append(&line, ' ')) nomem();
      if (!stralloc_cats(&line, *argv)) nomem();
    }
    task(line.s, line.len);
    nl();
    flush();
    return status;
  }

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
    if (!str_diff(line.s, "/exit") || !str_diff(line.s, "/quit")) break;
    if (!str_diff(line.s, "/help")) { help(); continue; }
    if (!str_diff(line.s, "/clear")) {
      while (conv.n) conv_drop(&conv);
      lastmain = nousage;
      pending = 0;
      newsession();  /* the old one stays saved, for -r */
      outs("(conversation cleared)\n");
      continue;
    }
    if (!str_diff(line.s, "/mode")) { showmode(); continue; }
    if (!str_diff(line.s, "/effort") || str_start(line.s, "/effort ")) {
      if (line.s[7] && !pickeffort(line.s + 8))
        outs("(effort is low, medium, high, xhigh, max or default)\n");
      else {
        outs("(effort: ");
        outs(effort ? effort : "the server's default");
        outs(")\n");
      }
      continue;
    }
    if (!str_diff(line.s, "/usage")) { showusage(); continue; }
    if (!str_diff(line.s, "/rules")) { showrules(); continue; }
    if (!str_diff(line.s, "/diff")) { showdiff(); continue; }
    if (!str_diff(line.s, "/commit")) {
      if (!ingit) { outs("(not in a git repository)\n"); continue; }
      status = 0;
      task(commitprompt, str_len(commitprompt));
      nl();
      flush();
      continue;
    }
    if (!str_diff(line.s, "/undo")) {
      if (undo()) save();
      else outs("(nothing to undo)\n");
      continue;
    }
    if (!str_diff(line.s, "/compact")) {
      intr = 0;
      if (!conv.n) outs("(nothing to compact)\n");
      else if (compact(0)) save();
      continue;
    }
    if (str_start(line.s, "/mode ")) {
      if (!pickmode(line.s + 6))
        outs("(modes are ask, rules, auto and yes)\n");
      else showmode();
      continue;
    }
    status = 0;
    task(line.s, line.len);
    nl();
    flush();
  }
  nl();
  flush();
  return status;
}
