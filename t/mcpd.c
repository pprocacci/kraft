/*
 * a toy MCP server for the tests: tools echo, add and fail, listed
 * over two pages.  it chatters (a log notification, a ping of its
 * own) before answering, as real servers may.
 *
 * mcpd speaks on stdio; mcpd -h portfile listens on 127.0.0.1 for
 * the streamable HTTP transport instead, wants "Authorization:
 * Bearer sesame" and a session, and streams the answers to tool
 * calls as events.
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

static stralloc o;  /* the messages to send, one per line */
static stralloc s;

static void
reply(const char *id, size_t idn, const char *result)
{
  stralloc_cats(&o, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\","
      "\"params\":{\"level\":\"info\",\"data\":\"hello\"}}\n");
  stralloc_cats(&o, "{\"jsonrpc\":\"2.0\",\"id\":\"s1\",\"method\":\"ping\"}\n");
  stralloc_cats(&o, "{\"jsonrpc\":\"2.0\",\"id\":");
  stralloc_catb(&o, id, idn);
  stralloc_cats(&o, ",\"result\":");
  stralloc_cats(&o, result);
  stralloc_cats(&o, "}\n");
}

static void
text(const char *id, size_t idn, const char *t, size_t n, int err)
{
  stralloc r = STRALLOC_ZERO;

  stralloc_copys(&r, "{\"content\":[{\"type\":\"text\",\"text\":");
  json_str(&r, t, n);
  stralloc_cats(&r, err ? "}],\"isError\":true}" : "},{\"type\":\"image\","
      "\"data\":\"AAAA\",\"mimeType\":\"image/png\"}]}");
  stralloc_0(&r);
  reply(id, idn, r.s);
  stralloc_free(&r);
}

/* one message from the client; 0 if it wants no answer */
static int
handle(const char *buf, size_t n)
{
  char num[64];
  const char *id, *m, *v;
  size_t idn, mn, vn;
  unsigned long a, b;

  if (!json_get(buf, n, "method", &m, &mn)) return 0;  /* our ping's answer */
  if (!json_get(buf, n, "id", &id, &idn)) return 0;  /* a notification */
  if (json_is(m, mn, "initialize"))
    reply(id, idn, "{\"protocolVersion\":\"2025-06-18\",\"capabilities\":"
        "{\"tools\":{}},\"serverInfo\":{\"name\":\"mcpd\",\"version\":\"1\"}}");
  else if (json_is(m, mn, "tools/list") &&
      !json_get(buf, n, "params.cursor", &v, &vn))
    reply(id, idn, "{\"tools\":[{\"name\":\"echo\",\"description\":"
        "\"Echo the text back.\",\"inputSchema\":{\"type\":\"object\","
        "\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":"
        "[\"text\"]}},{\"name\":\"add.two\",\"description\":\"Add a and b."
        "\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"a\":"
        "{\"type\":\"integer\"},\"b\":{\"type\":\"integer\"}}}}],"
        "\"nextCursor\":\"p2\"}");
  else if (json_is(m, mn, "tools/list"))
    reply(id, idn, "{\"tools\":[{\"name\":\"fail\",\"inputSchema\":"
        "{\"type\":\"object\"}}]}");
  else if (json_is(m, mn, "tools/call") &&
      json_get(buf, n, "params.name", &v, &vn) && json_is(v, vn, "echo")) {
    stralloc_copys(&s, "");
    if (json_get(buf, n, "params.arguments.text", &v, &vn))
      json_unstr(&s, v, vn);
    text(id, idn, s.s ? s.s : "", s.len, 0);
  } else if (json_is(m, mn, "tools/call") &&
      json_get(buf, n, "params.name", &v, &vn) && json_is(v, vn, "add.two")) {
    a = b = 0;
    if (json_get(buf, n, "params.arguments.a", &v, &vn)) a = strtoul(v, 0, 10);
    if (json_get(buf, n, "params.arguments.b", &v, &vn)) b = strtoul(v, 0, 10);
    snprintf(num, sizeof num, "%lu", a + b);
    text(id, idn, num, strlen(num), 0);
  } else if (json_is(m, mn, "tools/call"))
    text(id, idn, "it failed", 9, 1);
  else {
    stralloc_cats(&o, "{\"jsonrpc\":\"2.0\",\"id\":");
    stralloc_catb(&o, id, idn);
    stralloc_cats(&o, ",\"error\":{\"code\":-32601,\"message\":"
        "\"no such method\"}}\n");
  }
  return 1;
}

static void
stdio(void)
{
  char buf[65536];
  size_t n;

  while (fgets(buf, sizeof buf, stdin)) {
    n = strlen(buf);
    if (n && buf[n - 1] == '\n') --n;
    stralloc_copys(&o, "");
    handle(buf, n);
    fwrite(o.s, 1, o.len, stdout);
    fflush(stdout);
  }
}

static void
respond(int fd, const char *status, const char *type, const char *extra,
    const char *body, size_t n)
{
  char h[512];

  snprintf(h, sizeof h, "HTTP/1.1 %s\r\nContent-Type: %s\r\n%s"
      "Content-Length: %lu\r\nConnection: close\r\n\r\n", status, type,
      extra ? extra : "", (unsigned long)n);
  if (write(fd, h, strlen(h)) < 0 || write(fd, body, n) < 0) return;
}

static void
serve(int fd)
{
  static stralloc req, ev;
  char buf[65536], *e, *cl, *body, *last;
  size_t want = 0, hlen, i, j;
  ssize_t n;

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
  req.s[hlen - 2] = 0;  /* the headers alone, as a string */
  body = req.s + hlen;
  if (!strcasestr(req.s, "\nauthorization: bearer sesame\r")) {
    respond(fd, "401 Unauthorized", "text/plain", 0, "no", 2);
    return;
  }
  stralloc_copys(&o, "");
  if (!strstr(body, "\"initialize\"") &&
      !strcasestr(req.s, "\nmcp-session-id: sess-1\r")) {
    respond(fd, "404 Not Found", "text/plain", 0, "no session", 10);
    return;
  }
  if (!handle(body, want)) {
    respond(fd, "202 Accepted", "text/plain", 0, "", 0);
    return;
  }
  if (strstr(body, "\"tools/call\"")) {
    /* every message, as an event stream */
    stralloc_copys(&ev, "");
    for (i = 0; i < o.len; i = j + 1) {
      for (j = i; o.s[j] != '\n'; ++j) ;
      stralloc_cats(&ev, "event: message\ndata: ");
      stralloc_catb(&ev, o.s + i, j - i);
      stralloc_cats(&ev, "\n\n");
    }
    respond(fd, "200 OK", "text/event-stream", 0, ev.s, ev.len);
    return;
  }
  /* just the answer, the last line */
  o.s[--o.len] = 0;
  last = strrchr(o.s, '\n');
  last = last ? last + 1 : o.s;
  respond(fd, "200 OK", "application/json", strstr(body, "\"initialize\"") ?
      "Mcp-Session-Id: sess-1\r\n" : 0, last, strlen(last));
}

static void
http(const char *portfile)
{
  struct sockaddr_in sa;
  socklen_t sl = sizeof sa;
  struct pollfd p;
  FILE *f;
  int l, c, one = 1;

  signal(SIGPIPE, SIG_IGN);
  if ((l = socket(AF_INET, SOCK_STREAM, 0)) == -1) exit(111);
  setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(l, (struct sockaddr *)&sa, sizeof sa) == -1 || listen(l, 8) == -1 ||
      getsockname(l, (struct sockaddr *)&sa, &sl) == -1)
    exit(111);
  if (!(f = fopen(portfile, "w"))) exit(111);
  fprintf(f, "%d\n", ntohs(sa.sin_port));
  fclose(f);
  for (;;) {
    p.fd = l;
    p.events = POLLIN;
    if (poll(&p, 1, 60000) <= 0) exit(0);
    if ((c = accept(l, 0, 0)) == -1) continue;
    serve(c);
    shutdown(c, SHUT_WR);
    close(c);
  }
}

int
main(int argc, char **argv)
{
  if (argc == 3 && !strcmp(argv[1], "-h")) http(argv[2]);
  else stdio();
  return 0;
}
