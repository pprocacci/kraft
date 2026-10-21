/*
 * a toy MCP server for the tests: tools echo, add and fail, listed
 * over two pages.  it chatters (a log notification, a ping of its
 * own) before answering, as real servers may.
 *
 * mcpd speaks on stdio; mcpd -h portfile listens on 127.0.0.1 for
 * the streamable HTTP transport instead, wants "Authorization:
 * Bearer sesame" and a session, and streams the answers to tool
 * calls as events; mcpd -o portfile does the same with OAuth instead.
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
#include <openssl/evp.h>
#include "json.h"
#include "oauth.h"
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
        "{\"tools\":{},\"prompts\":{},\"resources\":{}},\"serverInfo\":"
        "{\"name\":\"mcpd\",\"version\":\"1\"}}");
  else if (json_is(m, mn, "prompts/list"))
    reply(id, idn, "{\"prompts\":[{\"name\":\"greet\",\"description\":"
        "\"Greet someone\",\"arguments\":[{\"name\":\"who\",\"required\":true},"
        "{\"name\":\"tone\"}]}]}");
  else if (json_is(m, mn, "prompts/get")) {
    stralloc_copys(&s, "Say hello to ");
    if (json_get(buf, n, "params.arguments.who", &v, &vn)) json_unstr(&s, v, vn);
    stralloc_cats(&s, " in a ");
    if (json_get(buf, n, "params.arguments.tone", &v, &vn)) json_unstr(&s, v, vn);
    else stralloc_cats(&s, "plain");
    stralloc_cats(&s, " way.");
    {
      stralloc r = STRALLOC_ZERO;

      stralloc_copys(&r, "{\"messages\":[{\"role\":\"user\",\"content\":"
          "{\"type\":\"text\",\"text\":");
      json_str(&r, s.s, s.len);
      stralloc_cats(&r, "}}]}");
      stralloc_0(&r);
      reply(id, idn, r.s);
      stralloc_free(&r);
    }
  } else if (json_is(m, mn, "resources/list"))
    reply(id, idn, "{\"resources\":[{\"uri\":\"toy://motd\",\"name\":"
        "\"Message of the day\",\"mimeType\":\"text/plain\"},{\"uri\":"
        "\"toy://logo\",\"name\":\"Logo\",\"mimeType\":\"image/png\"}]}");
  else if (json_is(m, mn, "resources/read") &&
      json_get(buf, n, "params.uri", &v, &vn) && json_is(v, vn, "toy://motd"))
    reply(id, idn, "{\"contents\":[{\"uri\":\"toy://motd\",\"mimeType\":"
        "\"text/plain\",\"text\":\"Be excellent to each other.\"}]}");
  else if (json_is(m, mn, "resources/read") &&
      json_get(buf, n, "params.uri", &v, &vn) && json_is(v, vn, "toy://logo"))
    reply(id, idn, "{\"contents\":[{\"uri\":\"toy://logo\",\"mimeType\":"
        "\"image/png\",\"blob\":\"AAAA\"}]}");
  else if (json_is(m, mn, "resources/read")) {
    stralloc_cats(&o, "{\"jsonrpc\":\"2.0\",\"id\":");
    stralloc_catb(&o, id, idn);
    stralloc_cats(&o, ",\"error\":{\"code\":-32002,\"message\":"
        "\"no such resource\"}}\n");
  }
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

/*
 * -o: OAuth, as MCP's authorization has it.  this server is both the
 * protected resource (/mcp) and its authorization server (/auth), with
 * registration, S256 PKCE checked with OpenSSL, refresh tokens, and
 * access tokens that last MCPD_USES requests (default many).
 */
static int oauth, port, uses = 1000000, left, ntok;
static char client[64], redirect[512], challenge[128], code[64];
static char access_[64], refresh[64];

/* base64url, unpadded, of the SHA-256 of s */
static void
s256(const char *s, size_t n, char *out)
{
  unsigned char md[EVP_MAX_MD_SIZE], b[128];
  unsigned int len;
  int k, i, j;

  EVP_Digest(s, n, md, &len, EVP_sha256(), 0);
  k = EVP_EncodeBlock(b, md, (int)len);
  for (i = j = 0; i < k; ++i)
    if (b[i] != '=') out[j++] = b[i] == '+' ? '-' : b[i] == '/' ? '_' : (char)b[i];
  out[j] = 0;
}

/* parameter name of query or form q (to its end, or a space), or "" */
static const char *
param(const char *q, const char *name)
{
  static stralloc v;
  size_t n = strcspn(q, " \r\n");

  if (!url_param(q, n, name, &v)) return "";
  return v.s;
}

static void
tokens(int fd)
{
  char b[512];

  ++ntok;
  snprintf(access_, sizeof access_, "tok%d", ntok);
  snprintf(refresh, sizeof refresh, "ref%d", ntok);
  left = uses;
  snprintf(b, sizeof b, "{\"access_token\":\"%s\",\"token_type\":\"Bearer\","
      "\"expires_in\":3600,\"refresh_token\":\"%s\"}", access_, refresh);
  respond(fd, "200 OK", "application/json", 0, b, strlen(b));
}

static void
oops(int fd, const char *error)
{
  char b[256];

  snprintf(b, sizeof b, "{\"error\":\"%s\"}", error);
  respond(fd, "400 Bad Request", "application/json", 0, b, strlen(b));
}

/* the OAuth side of a request; 1 if it is for /mcp and may go on */
static int
authorized(int fd, const char *h, const char *body, size_t n)
{
  char b[2048], me[64], tok[128], *p;
  const char *v;

  snprintf(me, sizeof me, "http://127.0.0.1:%d", port);
  if (!strncmp(h, "GET /.well-known/oauth-protected-resource/mcp ", 46)) {
    snprintf(b, sizeof b, "{\"resource\":\"%s/mcp\",\"authorization_servers\":"
        "[\"%s/auth\"],\"scopes_supported\":[\"toy.read\"]}", me, me);
    respond(fd, "200 OK", "application/json", 0, b, strlen(b));
    return 0;
  }
  if (!strncmp(h, "GET /.well-known/oauth-authorization-server/auth ", 49)) {
    snprintf(b, sizeof b, "{\"issuer\":\"%s/auth\",\"authorization_endpoint\":"
        "\"%s/auth/authorize\",\"token_endpoint\":\"%s/auth/token\","
        "\"registration_endpoint\":\"%s/auth/register\","
        "\"code_challenge_methods_supported\":[\"S256\"]}", me, me, me, me);
    respond(fd, "200 OK", "application/json", 0, b, strlen(b));
    return 0;
  }
  if (!strncmp(h, "POST /auth/register ", 20)) {
    const char *r;
    size_t rn;
    stralloc u = STRALLOC_ZERO;

    if (!json_get(body, n, "redirect_uris.0", &r, &rn) ||
        !json_unstr(&u, r, rn) || u.len >= sizeof redirect ||
        strncmp(u.s, "http://127.0.0.1:", 17)) {
      oops(fd, "invalid_redirect_uri");
      return 0;
    }
    memcpy(redirect, u.s, u.len);
    redirect[u.len] = 0;
    snprintf(client, sizeof client, "c%d", ntok + 1);
    snprintf(b, sizeof b, "{\"client_id\":\"%s\",\"token_endpoint_auth_method\":"
        "\"none\"}", client);
    respond(fd, "201 Created", "application/json", 0, b, strlen(b));
    return 0;
  }
  if (!strncmp(h, "GET /auth/authorize?", 20)) {
    char res[128];

    snprintf(res, sizeof res, "%s/mcp", me);
    p = (char *)h + 19;
    if (strcmp(param(p, "response_type"), "code") ||
        strcmp(param(p, "client_id"), client) ||
        strcmp(param(p, "redirect_uri"), redirect) ||
        strcmp(param(p, "code_challenge_method"), "S256") ||
        strcmp(param(p, "resource"), res) ||
        strcmp(param(p, "scope"), "toy.read") ||
        strlen(param(p, "code_challenge")) != 43 || !*param(p, "state")) {
      respond(fd, "400 Bad Request", "text/plain", 0, "bad request", 11);
      return 0;
    }
    snprintf(challenge, sizeof challenge, "%s", param(p, "code_challenge"));
    snprintf(code, sizeof code, "code%d", ntok + 1);
    /* the user said yes: back to kraft */
    snprintf(b, sizeof b, "Location: %s?code=%s&state=%s\r\n", redirect, code,
        param(p, "state"));
    respond(fd, "302 Found", "text/plain", b, "", 0);
    return 0;
  }
  if (!strncmp(h, "POST /auth/token ", 17)) {
    char s[128];
    stralloc f = STRALLOC_ZERO;

    stralloc_copyb(&f, body, n);
    stralloc_0(&f);
    if (!strcmp(param(f.s, "grant_type"), "authorization_code")) {
      s256(param(f.s, "code_verifier"), strlen(param(f.s, "code_verifier")), s);
      if (!*code || strcmp(param(f.s, "code"), code) ||
          strcmp(param(f.s, "client_id"), client) ||
          strcmp(param(f.s, "redirect_uri"), redirect) || strcmp(s, challenge)) {
        oops(fd, "invalid_grant");
        return 0;
      }
      *code = 0;  /* once only */
      tokens(fd);
    } else if (!strcmp(param(f.s, "grant_type"), "refresh_token")) {
      if (!*refresh || strcmp(param(f.s, "refresh_token"), refresh) ||
          strcmp(param(f.s, "client_id"), client)) {
        oops(fd, "invalid_grant");
        return 0;
      }
      tokens(fd);
    } else
      oops(fd, "unsupported_grant_type");
    return 0;
  }
  if (strncmp(h, "POST /mcp ", 10)) {
    respond(fd, "404 Not Found", "text/plain", 0, "", 0);
    return 0;
  }
  snprintf(tok, sizeof tok, "\nauthorization: bearer %s\r", access_);
  if (!*access_ || !strcasestr(h, tok) || left <= 0) {
    snprintf(b, sizeof b, "WWW-Authenticate: Bearer resource_metadata=\"%s/"
        ".well-known/oauth-protected-resource/mcp\", scope=\"toy.read\"\r\n", me);
    respond(fd, "401 Unauthorized", "text/plain", b, "log in", 6);
    return 0;
  }
  --left;
  (void)v;
  return 1;
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
  if (oauth) {
    if (!authorized(fd, req.s, body, want)) return;
  } else if (!strcasestr(req.s, "\nauthorization: bearer sesame\r")) {
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
  port = ntohs(sa.sin_port);
  fprintf(f, "%d\n", port);
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
  if (getenv("MCPD_USES")) uses = atoi(getenv("MCPD_USES"));
  if (argc == 3 && !strcmp(argv[1], "-o")) oauth = 1;
  if (argc == 3 && (!strcmp(argv[1], "-h") || oauth)) http(argv[2]);
  else stdio();
  return 0;
}
