#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "anth.h"
#include "api.h"
#include "buffer.h"
#include "byte.h"
#include "conv.h"
#include "file.h"
#include "http.h"
#include "json.h"
#include "oai.h"
#include "perm.h"
#include "proc.h"
#include "scan.h"
#include "sse.h"
#include "str.h"
#include "strerr.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"
#include "tool.h"
#include "ui.h"
#include "url.h"

#ifndef KRAFT_VERSION
#define KRAFT_VERSION "unknown"
#endif

#define FATAL "kraft: fatal: "
#define USAGE "kraft: usage: kraft [-ty] [-a anthropic|openai] [-m model] " \
    "[-n max_tokens] [-P ask|auto|yes] [-u url] [task ...]"
#define MAXSTEPS 100  /* model requests per task */

/* permission modes: when edits and commands need the user's approval */
#define MODE_ASK 0   /* always */
#define MODE_AUTO 1  /* unless perm_auto() allows them */
#define MODE_YES 2   /* never */

static const char *const modes[] = { "ask", "auto", "yes", 0 };

static const struct api apis[] = {
  { "anthropic", "/v1/messages", "ANTHROPIC_API_KEY", "x-api-key: ",
    "anthropic-version: 2023-06-01\r\n", anth_render, anth_event },
  { "openai", "/v1/chat/completions", "OPENAI_API_KEY",
    "Authorization: Bearer ", "", oai_render, oai_event },
};

static const struct api *api;
static const char *model, *base;
static unsigned long max = 16384;
static int showthink, mode, status;
static volatile sig_atomic_t intr;     /* ^C count during this task */
static volatile sig_atomic_t curfd = -1;  /* the connection being read */
static int allowed[32];  /* by tool index: approved for the session */
static char last = '\n';  /* last byte written to stdout */

static struct url url;
static struct conv conv;
static struct http http;
static struct sse sse;
static struct reply reply;
static stralloc host, path, hdrs, sys, body, event, data, line, tmp;

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

static void
nomem(void)
{
  strerr_die(111, FATAL, "out of memory", 0);
}

static void
out(const char *s, size_t n)
{
  if (!n) return;
  last = s[n - 1];
  if (buffer_put(buffer_1, s, n) == -1)
    strerr_sys(111, FATAL, "unable to write output", 0);
}

static void
outs(const char *s)
{
  out(s, str_len(s));
}

static void
flush(void)
{
  if (buffer_flush(buffer_1) == -1)
    strerr_sys(111, FATAL, "unable to write output", 0);
}

static void
nl(void)
{
  if (last != '\n') outs("\n");
}

static void
warn(const char *a, const char *b, const char *c)
{
  nl();
  flush();
  buffer_puts(buffer_2, "kraft: ");
  if (a) buffer_puts(buffer_2, a);
  if (b) buffer_puts(buffer_2, b);
  if (c) buffer_puts(buffer_2, c);
  buffer_puts(buffer_2, "\n");
  buffer_flush(buffer_2);
}

static void
warnsys(const char *a, const char *b)
{
  const char *e = strerror(errno);

  if (!stralloc_copys(&tmp, b) || !stralloc_cats(&tmp, ": ") ||
      !stralloc_cats(&tmp, e) || !stralloc_0(&tmp))
    nomem();
  warn(a, tmp.s, 0);
}

static const char *
env(const char *name, const char *dflt)
{
  const char *s = getenv(name);

  return (s && *s) ? s : dflt;
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
    if (errno != ENOENT) warnsys("unable to read ", tmp.s);
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
mksystem(void)
{
  struct utsname u;
  char cwd[PATH_MAX];
  stralloc md = STRALLOC_ZERO;

  if (!stralloc_copys(&sys, "You are kraft, a coding agent running in a "
      "terminal")) nomem();
  if (uname(&u) == 0) {
    if (!stralloc_cats(&sys, " on ") || !stralloc_cats(&sys, u.sysname) ||
        !stralloc_cats(&sys, " ") || !stralloc_cats(&sys, u.release))
      nomem();
  }
  if (!stralloc_cats(&sys, ". You help the user with software engineering "
      "tasks by reading and changing files and running commands"))
    nomem();
  if (getcwd(cwd, sizeof cwd)) {
    if (!stralloc_cats(&sys, " in the working directory ") ||
        !stralloc_cats(&sys, cwd))
      nomem();
  }
  if (!stralloc_cats(&sys, ".\n\n"
      "Look before you act: read files before editing them, and prefer "
      "edit_file to rewriting whole files. Run commands to build and test "
      "your changes when that helps. The user approves each edit and "
      "command and may decline; if they do, ask what they want instead. "
      "This is a plain terminal, so keep replies short and avoid Markdown "
      "tables. When a task is done, say briefly what you changed."))
    nomem();
  if (file_get("KRAFT.md", &md) && md.len) {
    if (!stralloc_cats(&sys, "\n\nProject instructions from KRAFT.md:\n\n") ||
        !stralloc_catb(&sys, md.s, md.len))
      nomem();
  }
  stralloc_free(&md);
  if (!stralloc_0(&sys)) nomem();
}

/* the body of a failed response, for the error message */
static void
slurp(void)
{
  char buf[1024];
  ssize_t r;

  if (!stralloc_copys(&body, "")) nomem();
  while (body.len < 4096 && (r = http_get(&http, buf, sizeof buf)) > 0)
    if (!stralloc_catb(&body, buf, (size_t)r)) nomem();
  if (!stralloc_0(&body)) nomem();
}

/*
 * after an interrupted reply, keeps only its text: a tool call or a
 * thinking block cut off part way would make the conversation invalid.
 */
static void
keeptext(void)
{
  struct msg *m;
  struct block *b;
  size_t i, j;

  if (!conv.n || conv.m[conv.n - 1].role != ROLE_ASSISTANT) return;
  m = &conv.m[conv.n - 1];
  for (i = j = 0; i < m->n; ++i) {
    b = &m->b[i];
    if (b->type == BLK_TEXT && b->text.len) {
      if (i != j) m->b[j] = *b;
      ++j;
    } else {
      stralloc_free(&b->id);
      stralloc_free(&b->name);
      stralloc_free(&b->text);
      stralloc_free(&b->aux);
    }
  }
  m->n = j;
  if (!j) conv_drop(&conv);
}

/*
 * sends the conversation and streams the reply into a new assistant
 * message, printing it as it arrives.  returns 1 on success.  on
 * failure the partial reply is dropped, so the conversation stays
 * valid, and 0 is returned.  if the user interrupts, whatever text
 * arrived is kept and -1 is returned.
 */
static int
request(void)
{
  struct req q;
  struct msg *m;
  int fd, r;

  q.model = model;
  q.max = max;
  q.system = sys.s;
  q.tools = tool_all;
  q.conv = &conv;
  if (!api->render(&body, &q)) nomem();
  if (intr) return -1;

  fd = tcp_connect(url.host.s, url.port.s);
  if (fd == -1) {
    if (intr) return -1;
    warnsys("unable to connect to ", base);
    goto fail;
  }
  if (url.tls && (fd = tls_connect(fd, url.host.s)) == -1) {
    if (intr) return -1;
    warnsys("unable to start TLS with ", base);
    goto fail;
  }
  curfd = fd;
  if (intr) goto stopped;  /* ^C came before the handler knew fd */
  if (!http_request(fd, "POST", host.s, path.s, hdrs.s, body.s, body.len)) {
    if (intr) goto stopped;
    warnsys("unable to send request to ", base);
    goto failfd;
  }
  r = http_response(&http, fd);
  if (r <= 0) {
    if (intr) goto stopped;
    if (r < 0) warnsys("unable to read response from ", base);
    else warn("malformed response from ", base, 0);
    goto failfd;
  }
  if (http.status != 200) {
    slurp();
    if (!stralloc_copys(&tmp, "") ||
        !stralloc_catulong(&tmp, (unsigned long)http.status) ||
        !stralloc_cats(&tmp, ": ") || !stralloc_0(&tmp))
      nomem();
    curfd = -1;
    close(fd);
    if (intr) { warn("interrupted", 0, 0); return -1; }
    warn("HTTP ", tmp.s, body.s);
    status = http.status < 500 ? 100 : 111;
    return 0;
  }

  if (!(m = conv_msg(&conv, ROLE_ASSISTANT))) nomem();
  sse.p = sse.n = 0;
  sse.eof = 0;
  reply.stop = STOP_NONE;
  for (;;) {
    r = sse_get(&sse, &http, &event, &data);
    if (r < 0) {
      if (intr) goto cut;
      warnsys("unable to read stream from ", base);
      goto drop;
    }
    if (r == 0) {
      if (intr) goto cut;  /* the handler shut the socket down */
      break;
    }
    if (!stralloc_copys(&reply.text, "") || !stralloc_copys(&reply.think, "") ||
        !stralloc_copys(&reply.err, ""))
      nomem();
    r = api->event(m, data.s, data.len, &reply);
    if (showthink && reply.think.len) {
      buffer_put(buffer_2, reply.think.s, reply.think.len);
      buffer_flush(buffer_2);
    }
    if (reply.text.len) {
      out(reply.text.s, reply.text.len);
      flush();
    }
    if (r == API_ERROR) {
      if (!stralloc_0(&reply.err)) nomem();
      warn("API error: ", reply.err.s, 0);
      goto drop;
    }
    if (r == API_DONE) break;
  }
  curfd = -1;
  close(fd);
  nl();
  flush();
  return 1;

cut:
  keeptext();
stopped:
  curfd = -1;
  close(fd);
  warn("interrupted", 0, 0);
  return -1;
drop:
  conv_drop(&conv);
failfd:
  curfd = -1;
  close(fd);
fail:
  status = 111;
  return 0;
}

static int
hastools(const struct msg *m)
{
  size_t i;

  for (i = 0; i < m->n; ++i)
    if (m->b[i].type == BLK_TOOL) return 1;
  return 0;
}

/* shows up to max lines of s, each with prefix p */
static void
lines(const char *p, const char *s, size_t n, unsigned long max)
{
  unsigned long count = 0, more = 0;
  size_t i, e;

  for (i = 0; i < n; i = e + 1) {
    e = i + byte_chr(s + i, n - i, '\n');
    if (count == max) { ++more; continue; }
    ++count;
    outs(p);
    out(s + i, e - i > 200 ? 200 : e - i);
    if (e - i > 200) outs("...");
    outs("\n");
  }
  if (more) {
    outs(p);
    outs("... (");
    if (!stralloc_copys(&tmp, "") || !stralloc_catulong(&tmp, more) ||
        !stralloc_0(&tmp))
      nomem();
    outs(tmp.s);
    outs(more == 1 ? " more line)\n" : " more lines)\n");
  }
}

/* "* name(what)": a one-line description of a tool call */
static void
announce(const struct block *b, const char *in, size_t n)
{
  static const char *keys[] = { "command", "path", "pattern", 0 };
  size_t i, k;

  nl();
  outs("* ");
  out(b->name.s, b->name.len);
  outs("(");
  for (i = 0; keys[i]; ++i)
    if (tool_arg(in, n, keys[i], &tmp)) break;
  if (keys[i]) {
    for (k = 0; k < tmp.len && k < 100; ++k)
      out(tmp.s[k] == '\n' ? " " : tmp.s + k, 1);
    if (tmp.len > 100) outs("...");
  }
  outs(")\n");
}

/* asks whether tool t may run with input in; 1 if allowed */
static int
approve(const struct tool *t, const char *in, size_t n)
{
  size_t i = (size_t)(t - tool_all);
  int c;

  if (!t->ask || mode == MODE_YES || allowed[i]) return 1;
  if (mode == MODE_AUTO) {
    if (perm_auto(t->name, in, n, &tmp)) return 1;
    if (!stralloc_0(&tmp)) nomem();
    outs("  (auto mode asks: ");
    outs(tmp.s);
    outs(")\n");
  }
  if (!str_diff(t->name, "bash")) {
    if (tool_arg(in, n, "command", &tmp)) lines("  $ ", tmp.s, tmp.len, 20);
  } else if (!str_diff(t->name, "edit_file")) {
    if (tool_arg(in, n, "old_string", &tmp)) lines("  - ", tmp.s, tmp.len, 20);
    if (tool_arg(in, n, "new_string", &tmp)) lines("  + ", tmp.s, tmp.len, 20);
  } else if (!str_diff(t->name, "write_file")) {
    if (tool_arg(in, n, "content", &tmp)) lines("  + ", tmp.s, tmp.len, 20);
  }
  flush();
  c = ui_ask("  Allow? [y]es, [n]o, [a]lways for this session: ");
  last = '\n';
  if (!c) {
    outs("  (declined: no terminal to ask on; see -P)\n");
    return 0;
  }
  if (c == 'a') allowed[i] = 1;
  return c != 'n';
}

/*
 * runs the tool calls in the last assistant message and puts their
 * results in a new user message.  if cut is set the reply was
 * truncated, so the calls are answered with errors instead.  every
 * call gets a result, even after an interrupt, as the APIs require.
 * returns 0 if the user interrupted.
 */
static int
runtools(int cut)
{
  const struct tool *t;
  struct msg *u, *a;
  struct block *b, *res;
  const char *in;
  size_t i, n;

  if (!(u = conv_msg(&conv, ROLE_USER))) nomem();
  a = &conv.m[conv.n - 2];
  for (i = 0; i < a->n; ++i) {
    b = &a->b[i];
    if (b->type != BLK_TOOL) continue;
    if (!(res = conv_block(u, BLK_RESULT))) nomem();
    if (!stralloc_copyb(&res->id, b->id.s, b->id.len)) nomem();
    in = b->text.len ? b->text.s : "{}";
    n = b->text.len ? b->text.len : 2;
    for (t = tool_all; t->name; ++t)
      if (str_len(t->name) == b->name.len &&
          !byte_diff(t->name, b->name.len, b->name.s))
        break;

    res->iserr = 1;
    if (intr) {
      stralloc_copys(&res->text, "not run: the user interrupted");
      continue;
    }
    announce(b, in, n);
    if (cut)
      stralloc_copys(&res->text, "not run: the reply was cut off before "
          "this call was complete");
    else if (!t->name)
      stralloc_copys(&res->text, "there is no tool by that name");
    else if (!json_object(in, n))
      stralloc_copys(&res->text, "the input was not a valid JSON object");
    else if (!approve(t, in, n))
      stralloc_copys(&res->text, intr ? "not run: the user interrupted" :
          "the user declined this call; ask them what to do instead");
    else {
      res->iserr = !t->run(in, n, &res->text);
      if (intr) {
        stralloc_cats(&res->text, "\n[interrupted by the user]");
        res->iserr = 1;
      }
    }
    lines(res->iserr ? "  ! " : "  | ", res->text.s, res->text.len, 4);
    flush();
  }
  if (intr) warn("interrupted", 0, 0);
  return !intr;
}

/* tells the model, in the user's next turn, that it was cut short */
static void
interrupted(void)
{
  struct msg *m;
  struct block *b;

  if (!(m = conv_msg(&conv, ROLE_USER))) nomem();
  if (!(b = conv_block(m, BLK_TEXT))) nomem();
  if (!stralloc_copys(&b->text, "[The user interrupted. Stop what you were "
      "doing and wait for their next message.]"))
    nomem();
  status = 130;
}

/* carries out one task: the user's words, then model and tools in turn */
static void
task(const char *s, size_t n)
{
  struct msg *m;
  struct block *b;
  int step, r;

  intr = 0;
  if (!(m = conv_msg(&conv, ROLE_USER))) nomem();
  if (!(b = conv_block(m, BLK_TEXT))) nomem();
  if (!stralloc_copyb(&b->text, s, n)) nomem();

  for (step = 0; step < MAXSTEPS; ++step) {
    r = request();
    if (r < 0) { interrupted(); return; }
    if (!r) return;
    m = &conv.m[conv.n - 1];
    if (reply.stop == STOP_REFUSAL) warn("the model declined this request", 0, 0);
    if (!hastools(m)) {
      if (reply.stop == STOP_MAX)
        warn("the reply was cut off at max_tokens; raise it with -n", 0, 0);
      return;
    }
    if (!runtools(reply.stop == STOP_MAX)) { interrupted(); return; }
    if (reply.stop == STOP_MAX) {
      warn("the reply was cut off at max_tokens; raise it with -n", 0, 0);
      return;
    }
  }
  warn("stopped: too many steps for one task", 0, 0);
}

static void
help(void)
{
  outs("Type a task and press return. Commands:\n"
      "  /clear  forget the conversation\n"
      "  /mode   show or set when edits and commands need approval:\n"
      "          /mode ask    always ask\n"
      "          /mode auto   ask only when the auto-mode rules object\n"
      "          /mode yes    never ask\n"
      "  /help   show this\n"
      "  /exit   quit (or press ctrl-d)\n"
      "^C stops the current reply or command.\n");
}

static int
pickmode(const char *s)
{
  int i;

  for (i = 0; modes[i]; ++i)
    if (!str_diff(s, modes[i])) { mode = i; return 1; }
  return 0;
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
  int opt;
  size_t i;
  struct sigaction sa;
  int r;

  signal(SIGPIPE, SIG_IGN);
  sa.sa_handler = onint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  /* no SA_RESTART: blocked calls must return EINTR */
  sigaction(SIGINT, &sa, 0);
  aname = env("KRAFT_API", "anthropic");
  model = env("KRAFT_MODEL", 0);
  base = env("KRAFT_URL", "http://127.0.0.1:1234");
  if (!pickmode(env("KRAFT_MODE", "ask")))
    strerr_die(100, FATAL, "KRAFT_MODE must be ask, auto or yes", 0);
  while ((opt = getopt(argc, argv, "a:m:n:P:tu:y")) != -1)
    switch (opt) {
    case 'a': aname = optarg; break;
    case 'm': model = optarg; break;
    case 'n':
      if (!*optarg || scan_ulong(optarg, &max) != str_len(optarg) || !max)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 'P': if (!pickmode(optarg)) strerr_die(100, USAGE, 0, 0); break;
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
  if (!url_parse(&url, base)) strerr_die(100, FATAL, "bad URL: ", base);

  /* accept base URLs given with or without a trailing /v1 */
  i = url.path.len - 1;
  if (i >= 3 && !byte_diff(url.path.s + i - 3, 3, "/v1")) i -= 3;
  if (!stralloc_copyb(&path, url.path.s, i)) nomem();
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
  if ((key = readkey())) {
    if (!stralloc_cats(&hdrs, api->keyhdr) || !stralloc_cats(&hdrs, key) ||
        !stralloc_cats(&hdrs, "\r\n"))
      nomem();
  }
  if (!stralloc_0(&hdrs)) nomem();
  mksystem();

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
    outs("kraft " KRAFT_VERSION ": ");
    outs(model);
    outs(" at ");
    outs(base);
    outs(", permission mode ");
    outs(modes[mode]);
    outs(". /help for help.\n");
  }
  while ((r = ui_readline(&line, mode == MODE_AUTO ? "auto> " :
      mode == MODE_YES ? "yes> " : "> "))) {
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
      outs("(conversation cleared)\n");
      continue;
    }
    if (!str_diff(line.s, "/mode")) { showmode(); continue; }
    if (str_start(line.s, "/mode ")) {
      if (!pickmode(line.s + 6)) outs("(modes are ask, auto and yes)\n");
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
