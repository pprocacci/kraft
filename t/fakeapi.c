/*
 * fakeapi script logdir portfile: a model server for the tests.  it
 * listens on 127.0.0.1, writes its port to portfile, and answers each
 * POST with the script's next reply, streamed as the Anthropic
 * Messages API or Chat Completions would (by the request's path).
 * request bodies are saved as logdir/1, logdir/2, ...  a GET of a
 * model gives a context window of 200000.
 *
 * the script, one directive per line, a reply starting at each
 * "reply" or "status":
 *
 *   reply                  a new reply
 *   text WORDS             a text block (\n is a newline)
 *   think WORDS            a thinking block
 *   tool NAME JSON         a tool call
 *   stop REASON            end_turn, tool_use or max_tokens (by default
 *                          tool_use if there is a call, else end_turn)
 *   status CODE            an error reply instead, with Retry-After: 1
 *
 * once the script is used up, POSTs get a 500.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "json.h"
#include "stralloc.h"

#define MAXREPLY 64
#define MAXBLK 16

struct blk { int type; char *name; char *s; };  /* 't'ext, 'k' think, 'u'se */
struct reply { int status; const char *stop; int n; struct blk b[MAXBLK]; };

static struct reply r[MAXREPLY];
static int nr, next, ids;
static stralloc o;

static void
die(const char *s)
{
  fprintf(stderr, "fakeapi: %s\n", s);
  exit(111);
}

static void
load(const char *fn)
{
  char line[65536], *p, *q;
  struct reply *cur = 0;
  struct blk *b;
  FILE *f = fopen(fn, "r");
  size_t n;

  if (!f) die("cannot open the script");
  while (fgets(line, sizeof line, f)) {
    n = strlen(line);
    if (n && line[n - 1] == '\n') line[--n] = 0;
    if (!n || *line == '#') continue;
    p = strchr(line, ' ');
    if (p) *p++ = 0; else p = line + n;
    if (!strcmp(line, "reply") || !strcmp(line, "status")) {
      if (nr == MAXREPLY) die("too many replies");
      cur = &r[nr++];
      cur->status = !strcmp(line, "status") ? atoi(p) : 200;
      continue;
    }
    if (!cur) die("a directive before the first reply");
    if (!strcmp(line, "stop")) { cur->stop = strdup(p); continue; }
    if (cur->n == MAXBLK) die("too many blocks");
    b = &cur->b[cur->n++];
    if (!strcmp(line, "tool")) {
      b->type = 'u';
      q = strchr(p, ' ');
      if (!q) die("tool needs a name and its input");
      *q++ = 0;
      b->name = strdup(p);
      b->s = strdup(q);
      continue;
    }
    b->type = !strcmp(line, "text") ? 't' : !strcmp(line, "think") ? 'k' : 0;
    if (!b->type) die("unknown directive");
    /* \n means a newline */
    for (q = p; (q = strstr(q, "\\n")); ) { *q = '\n'; memmove(q + 1, q + 2, strlen(q + 2) + 1); }
    b->s = strdup(p);
  }
  fclose(f);
}


static void
event(const char *name, const char *data)
{
  if (name) {
    stralloc_cats(&o, "event: ");
    stralloc_cats(&o, name);
    stralloc_cats(&o, "\n");
  }
  stralloc_cats(&o, "data: ");
  stralloc_cats(&o, data);
  stralloc_cats(&o, "\n\n");
}

static stralloc d;

static void
dstart(void)
{
  stralloc_copys(&d, "");
}

static void
dcat(const char *s)
{
  stralloc_cats(&d, s);
}

static void
dnum(unsigned long u)
{
  stralloc_catulong(&d, u);
}

static void
djstr(const char *s, size_t n)
{
  json_str(&d, s, n);
}

static const char *
dz(void)
{
  stralloc_0(&d);
  return d.s;
}

static const char *
stopof(const struct reply *x, int oai)
{
  int i, tools = 0;
  const char *s;

  for (i = 0; i < x->n; ++i) tools |= x->b[i].type == 'u';
  s = x->stop ? x->stop : tools ? "tool_use" : "end_turn";
  if (!oai) return s;
  return !strcmp(s, "tool_use") ? "tool_calls" : !strcmp(s, "max_tokens") ?
      "length" : "stop";
}

static void
anthropic(const struct reply *x)
{
  size_t k, len, step;
  int i;

  event("message_start", "{\"type\":\"message_start\",\"message\":{\"id\":"
      "\"msg_fake\",\"type\":\"message\",\"role\":\"assistant\",\"model\":"
      "\"fake\",\"content\":[],\"stop_reason\":null,\"usage\":"
      "{\"input_tokens\":100,\"cache_read_input_tokens\":0,"
      "\"cache_creation_input_tokens\":0,\"output_tokens\":1}}}");
  event("ping", "{\"type\":\"ping\"}");
  for (i = 0; i < x->n; ++i) {
    const struct blk *b = &x->b[i];

    dstart();
    dcat("{\"type\":\"content_block_start\",\"index\":");
    dnum((unsigned long)i);
    if (b->type == 't') dcat(",\"content_block\":{\"type\":\"text\",\"text\":\"\"}}");
    else if (b->type == 'k')
      dcat(",\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\"}}");
    else {
      dcat(",\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_");
      dnum((unsigned long)++ids);
      dcat("\",\"name\":");
      djstr(b->name, strlen(b->name));
      dcat(",\"input\":{}}");
    }
    event("content_block_start", dz());
    len = strlen(b->s);
    step = b->type == 'u' ? 7 : 5;
    for (k = 0; k < len; k += step) {
      dstart();
      dcat("{\"type\":\"content_block_delta\",\"index\":");
      dnum((unsigned long)i);
      dcat(b->type == 't' ? ",\"delta\":{\"type\":\"text_delta\",\"text\":" :
          b->type == 'k' ? ",\"delta\":{\"type\":\"thinking_delta\",\"thinking\":" :
          ",\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":");
      djstr(b->s + k, len - k < step ? len - k : step);
      dcat("}}");
      event("content_block_delta", dz());
    }
    if (b->type == 'k') {
      dstart();
      dcat("{\"type\":\"content_block_delta\",\"index\":");
      dnum((unsigned long)i);
      dcat(",\"delta\":{\"type\":\"signature_delta\",\"signature\":\"c2ln\"}}");
      event("content_block_delta", dz());
    }
    dstart();
    dcat("{\"type\":\"content_block_stop\",\"index\":");
    dnum((unsigned long)i);
    dcat("}");
    event("content_block_stop", dz());
  }
  dstart();
  dcat("{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"");
  dcat(stopof(x, 0));
  dcat("\"},\"usage\":{\"output_tokens\":20}}");
  event("message_delta", dz());
  event("message_stop", "{\"type\":\"message_stop\"}");
}

static void
openai(const struct reply *x)
{
  size_t k, len;
  int i, calls = 0;

  for (i = 0; i < x->n; ++i) {
    const struct blk *b = &x->b[i];

    len = strlen(b->s);
    if (b->type == 'u') {
      dstart();
      dcat("{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":");
      dnum((unsigned long)calls);
      dcat(",\"id\":\"call_");
      dnum((unsigned long)++ids);
      dcat("\",\"type\":\"function\",\"function\":{\"name\":");
      djstr(b->name, strlen(b->name));
      dcat(",\"arguments\":\"\"}}]}}]}");
      event(0, dz());
      for (k = 0; k < len; k += 7) {
        dstart();
        dcat("{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":");
        dnum((unsigned long)calls);
        dcat(",\"function\":{\"arguments\":");
        djstr(b->s + k, len - k < 7 ? len - k : 7);
        dcat("}}]}}]}");
        event(0, dz());
      }
      ++calls;
      continue;
    }
    for (k = 0; k < len; k += 5) {
      dstart();
      dcat(b->type == 't' ? "{\"choices\":[{\"index\":0,\"delta\":{\"content\":" :
          "{\"choices\":[{\"index\":0,\"delta\":{\"reasoning_content\":");
      djstr(b->s + k, len - k < 5 ? len - k : 5);
      dcat("}}]}");
      event(0, dz());
    }
  }
  dstart();
  dcat("{\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"");
  dcat(stopof(x, 1));
  dcat("\"}]}");
  event(0, dz());
  event(0, "{\"choices\":[],\"usage\":{\"prompt_tokens\":100,"
      "\"completion_tokens\":20,\"total_tokens\":120}}");
  event(0, "[DONE]");
}

static void
respond(int fd, int status, const char *type, const char *extra)
{
  char h[512];

  snprintf(h, sizeof h, "HTTP/1.1 %d Fake\r\nContent-Type: %s\r\n%s"
      "Content-Length: %lu\r\nConnection: close\r\n\r\n", status, type,
      extra ? extra : "", (unsigned long)o.len);
  if (write(fd, h, strlen(h)) < 0 || write(fd, o.s, o.len) < 0) return;
}

static void
serve(int fd, const char *logdir)
{
  static stralloc req;
  static unsigned long nlog;
  char buf[65536], fn[1024], *e, *cl;
  size_t want = 0, hlen;
  ssize_t n;
  FILE *f;

  stralloc_copys(&req, "");
  for (;;) {
    if ((n = read(fd, buf, sizeof buf)) <= 0) return;
    stralloc_catb(&req, buf, (size_t)n);
    stralloc_0(&req);
    --req.len;
    if ((e = strstr(req.s, "\r\n\r\n"))) {
      hlen = (size_t)(e - req.s) + 4;
      for (cl = req.s; (cl = strchr(cl, '\n')); )
        if (!strncasecmp(++cl, "content-length:", 15)) want = strtoul(cl + 15, 0, 10);
      if (req.len >= hlen + want) break;
    }
  }
  stralloc_copys(&o, "");
  if (!strncmp(req.s, "GET ", 4)) {
    if (strstr(req.s, "/models/")) {
      stralloc_cats(&o, "{\"max_input_tokens\":200000}");
      respond(fd, 200, "application/json", 0);
    } else
      respond(fd, 404, "text/plain", 0);
    return;
  }
  snprintf(fn, sizeof fn, "%s/%lu", logdir, ++nlog);
  if ((f = fopen(fn, "w"))) {
    fwrite(req.s + hlen, 1, want, f);
    fclose(f);
  }
  if (next == nr) {
    stralloc_cats(&o, "{\"type\":\"error\",\"error\":{\"type\":\"api_error\","
        "\"message\":\"the script is used up\"}}");
    respond(fd, 500, "application/json", 0);
    return;
  }
  if (r[next].status != 200) {
    stralloc_cats(&o, "{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\","
        "\"message\":\"scripted failure\"}}");
    respond(fd, r[next++].status, "application/json", "Retry-After: 1\r\n");
    return;
  }
  if (strstr(req.s, "/chat/completions")) openai(&r[next++]);
  else anthropic(&r[next++]);
  respond(fd, 200, "text/event-stream", 0);
}

int
main(int argc, char **argv)
{
  struct sockaddr_in sa;
  socklen_t sl = sizeof sa;
  struct pollfd p;
  FILE *f;
  int s, c, one = 1;

  if (argc != 4) die("usage: fakeapi script logdir portfile");
  signal(SIGPIPE, SIG_IGN);
  load(argv[1]);
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) die("socket");
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(s, (struct sockaddr *)&sa, sizeof sa) == -1 || listen(s, 8) == -1 ||
      getsockname(s, (struct sockaddr *)&sa, &sl) == -1)
    die("cannot listen");
  if (!(f = fopen(argv[3], "w"))) die("cannot write the port file");
  fprintf(f, "%d\n", ntohs(sa.sin_port));
  fclose(f);
  for (;;) {
    p.fd = s;
    p.events = POLLIN;
    if (poll(&p, 1, 60000) <= 0) return 0;  /* idle: the test is over */
    if ((c = accept(s, 0, 0)) == -1) continue;
    serve(c, argv[2]);
    shutdown(c, SHUT_WR);
    close(c);
  }
}
