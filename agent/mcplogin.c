#include "agent.h"

#define WAITSECS 600
#define PING "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}"

/* does s (n bytes) contain t? */
static int
has(const char *s, size_t n, const char *t)
{
  size_t k = str_len(t), i;

  for (i = 0; i + k <= n; ++i)
    if (!byte_diff(s + i, k, t)) return 1;
  return 0;
}

/* the string at key in JSON j (n bytes), NUL-terminated; 1 if found */
static int
jstr(const char *j, size_t n, const char *key, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(j, n, key, &v, &vn) || *v != '"') return 0;
  if (!stralloc_copys(out, "") || !json_unstr(out, v, vn) || !stralloc_0(out))
    nomem();
  --out->len;
  return out->len > 0;
}

/* GETs JSON from url into out; 1 if the answer was a JSON object */
static int
getjson(const char *url, stralloc *out)
{
  static struct http h;
  static stralloc why;

  return http_fetch("GET", url, "Accept: application/json\r\n", 0, 0, &h, out,
      &why) && h.status == 200 && json_object(out->s, out->len);
}

/* scheme://host[:port] of url (http or https), NUL-terminated */
static int
origin(const char *url, stralloc *out)
{
  size_t i = str_start(url, "https://") ? 8 : str_start(url, "http://") ? 7 : 0;

  if (!i) return 0;
  i += byte_chr(url + i, str_len(url + i), '/');
  if (!stralloc_copyb(out, url, i) || !stralloc_0(out)) nomem();
  --out->len;
  return 1;
}

/* the path of url after its origin, without a trailing slash */
static void
pathof(const char *url, size_t olen, stralloc *out)
{
  size_t n = str_len(url);

  while (n > olen && url[n - 1] == '/') --n;
  if (!stralloc_copyb(out, url + olen, n - olen) || !stralloc_0(out)) nomem();
  --out->len;
}

/* a/.well-known/what then b, onto out, NUL-terminated */
static void
wellknown(stralloc *out, const stralloc *a, const char *what,
    const stralloc *b)
{
  if (!stralloc_copy(out, a) || !stralloc_cats(out, "/.well-known/") ||
      !stralloc_cats(out, what) || !stralloc_cat(out, b) || !stralloc_0(out))
    nomem();
}

/* opens url with the user's browser, if they have said which */
static void
openurl(const char *url)
{
  static stralloc cmd;
  const char *b = browser ? browser : env("BROWSER", 0);
  pid_t pid;
  int fd;

  if (!b || !*b) return;
  if (!stralloc_copys(&cmd, b) || !stralloc_cats(&cmd, " \"$KRAFT_URL\"") ||
      !stralloc_0(&cmd))
    nomem();
  if ((pid = fork()) == -1) return;
  if (pid == 0) {
    /* twice, so that nothing waits for it */
    if (fork() != 0) _exit(0);
    setsid();
    if ((fd = open("/dev/null", O_RDWR)) != -1) {
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
    }
    setenv("KRAFT_URL", url, 1);
    execl("/bin/sh", "sh", "-c", cmd.s, (char *)0);
    _exit(127);
  }
  waitpid(pid, 0, 0);
}

/*
 * what the authorization server sent back, in redirect URL or query
 * q: 1 with the code, 0 with the reason, -1 if it is not that at all
 */
static int
answer(const char *q, size_t n, const stralloc *state, stralloc *code,
    stralloc *why)
{
  static stralloc s, e;

  if (url_param(q, n, "error", &e)) {
    if (!stralloc_copys(why, "the authorization server said ") ||
        !stralloc_cat(why, &e))
      nomem();
    if (url_param(q, n, "error_description", &e) &&
        (!stralloc_cats(why, ": ") || !stralloc_cat(why, &e)))
      nomem();
    return 0;
  }
  if (!url_param(q, n, "code", code)) return -1;
  if (!url_param(q, n, "state", &s) || s.len != state->len ||
      byte_diff(s.s, s.len, state->s)) {
    if (!stralloc_copys(why, "the answer was not for this login (its state "
        "did not match)"))
      nomem();
    return 0;
  }
  return 1;
}

/* the browser's visit to the redirect URI, on listening socket ls */
static int
visit(int ls, const stralloc *state, stralloc *code, stralloc *why)
{
  static char ok[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
      "charset=utf-8\r\nConnection: close\r\n\r\n<!doctype html><title>kraft"
      "</title><p>Logged in. You can close this tab and go back to kraft.";
  static char bad[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: "
      "text/html; charset=utf-8\r\nConnection: close\r\n\r\n<!doctype html>"
      "<title>kraft</title><p>The login did not work; kraft says why.";
  static char none[] = "HTTP/1.1 404 Not Found\r\nConnection: "
      "close\r\nContent-Length: 0\r\n\r\n";
  static stralloc req;
  char b[4096];
  size_t e, sp;
  ssize_t r;
  int fd, got = -1;

  if ((fd = accept(ls, 0, 0)) == -1) return -1;
  if (!stralloc_copys(&req, "")) nomem();
  while (req.len < 16384 &&
      (e = byte_chr(req.s, req.len, '\n')) == req.len) {
    struct pollfd p;

    p.fd = fd;
    p.events = POLLIN;
    if (poll(&p, 1, 5000) <= 0 || (r = read(fd, b, sizeof b)) <= 0) break;
    if (!stralloc_catb(&req, b, (size_t)r)) nomem();
  }
  /* "GET /callback?code=...&state=... HTTP/1.1" */
  e = byte_chr(req.s, req.len, '\n');
  if (e < req.len && req.len > 4 && !byte_diff(req.s, 4, "GET ")) {
    sp = 4 + byte_chr(req.s + 4, e - 4, ' ');
    if (sp - 4 >= 9 && !byte_diff(req.s + 4, 9, "/callback"))
      got = answer(req.s + 4, sp - 4, state, code, why);
  }
  if (got < 0) buffer_unixwrite(fd, none, sizeof none - 1);
  else if (got) buffer_unixwrite(fd, ok, sizeof ok - 1);
  else buffer_unixwrite(fd, bad, sizeof bad - 1);
  close(fd);
  return got;
}

/*
 * waits for the code: the browser coming back to listening socket
 * ls, or, from a terminal, the address it was sent to pasted in (for
 * when the browser is on another machine).  1 with the code, or 0.
 */
static int
await(int ls, const stralloc *state, stralloc *code, stralloc *why)
{
  static stralloc line;
  struct pollfd p[2];
  time_t end = time(0) + WAITSECS;
  char b[4096];
  ssize_t r;
  int tty = isatty(0), got;

  if (!stralloc_copys(&line, "")) nomem();
  while (time(0) < end) {
    if (intr) {
      if (!stralloc_copys(why, "interrupted")) nomem();
      return 0;
    }
    p[0].fd = ls;
    p[0].events = POLLIN;
    p[1].fd = tty ? 0 : -1;
    p[1].events = POLLIN;
    if (poll(p, 2, 1000) <= 0) continue;
    if (p[0].revents & POLLIN) {
      got = visit(ls, state, code, why);
      if (got >= 0) return got;
    }
    if (tty && (p[1].revents & (POLLIN | POLLHUP))) {
      r = read(0, b, sizeof b);
      if (r == 0) {
        /* control-D */
        if (!stralloc_copys(why, "cancelled")) nomem();
        return 0;
      }
      if (r < 0) {
        tty = 0;
        continue;
      }
      if (!stralloc_catb(&line, b, (size_t)r)) nomem();
      while (line.len && byte_chr(line.s, line.len, '\n') < line.len) {
        size_t e = byte_chr(line.s, line.len, '\n');

        got = answer(line.s, e, state, code, why);
        byte_copy(line.s, line.len - e - 1, line.s + e + 1);
        line.len -= e + 1;
        if (got >= 0) return got;
        if (e) outs("  (that is not the address with the code in it)\n");
        flush();
      }
    }
  }
  if (!stralloc_copys(why, "no answer in ten minutes")) nomem();
  return 0;
}

/* registers kraft with the authorization server's endpoint ep */
static int
regist(const stralloc *ep, const stralloc *redirect, struct mcpcred *c,
    stralloc *why)
{
  static struct http h;
  static stralloc req, res;

  if (!stralloc_copys(&req, "{\"client_name\":\"kraft\",\"redirect_uris\":[") ||
      !json_str(&req, redirect->s, redirect->len) ||
      !stralloc_cats(&req, "],\"grant_types\":[\"authorization_code\","
      "\"refresh_token\"],\"response_types\":[\"code\"],"
      "\"token_endpoint_auth_method\":\"none\"}"))
    nomem();
  if (!http_fetch("POST", ep->s, "Content-Type: application/json\r\n"
      "Accept: application/json\r\n", req.s, req.len, &h, &res, why))
    return 0;
  if ((h.status != 200 && h.status != 201) ||
      !jstr(res.s, res.len, "client_id", &c->clientid)) {
    if (!stralloc_copys(why, "the authorization server would not register "
        "kraft (HTTP ") || !stralloc_catulong(why, (unsigned long)h.status) ||
        !stralloc_cats(why, "); see mcp_client in kraft(1)"))
      nomem();
    return 0;
  }
  if (!jstr(res.s, res.len, "client_secret", &c->secret) &&
      !stralloc_copys(&c->secret, ""))
    nomem();
  if (!jstr(res.s, res.len, "token_endpoint_auth_method", &c->method) &&
      !stralloc_copys(&c->method, c->secret.len ? "client_secret_basic" : "none"))
    nomem();
  return 1;
}

/*
 * logs in to HTTP server k with OAuth, as MCP's authorization says:
 * find its authorization server, register with it if need be, send
 * the user there with a PKCE challenge, and trade the code that comes
 * back for tokens, which are saved.  returns 1 if it worked.
 */
int
mcplogin(unsigned long k)
{
  static struct mcpcred c;
  static struct http h;
  static stralloc why, chal, prm, scope, org, path, u, j, issuer, authep, regep;
  static stralloc redirect, verifier, challenge, state, url, code, form;
  struct mcp *m = &mcps[k];
  struct sockaddr_in sa;
  socklen_t sl = sizeof sa;
  const char *v;
  size_t vn, i;
  int ls = -1, one = 1, found, ok = 0;

  if (!m->http) {
    kwarn("MCP server ", m->name, " is a command, not a URL: no login needed");
    return 0;
  }
  mcpcredload(k, &c);  /* the client may be the one registered before */
  if (!stralloc_copys(&why, "") || !stralloc_copys(&scope, "") ||
      !stralloc_copys(&prm, ""))
    nomem();

  /* 1: the server's challenge, which may say where to look */
  if (!http_fetch("POST", m->cmd, "Content-Type: application/json\r\n"
      "Accept: application/json, text/event-stream\r\n",
      PING, str_len(PING), &h, &j, &why))
    goto fail;
  if (!stralloc_copy(&chal, &h.auth)) nomem();
  www_param(chal.s, chal.len ? chal.len - 1 : 0, "resource_metadata", &prm);
  www_param(chal.s, chal.len ? chal.len - 1 : 0, "scope", &scope);

  /* 2: the protected resource's metadata, for its authorization server */
  if (!origin(m->cmd, &org)) goto fail;
  pathof(m->cmd, org.len, &path);
  found = 0;
  for (i = 0; i < 3 && !found; ++i) {
    if (i == 0 && !prm.len) continue;
    if (i == 0 && !stralloc_copy(&u, &prm)) nomem();
    if (i == 0 && !stralloc_0(&u)) nomem();
    if (i == 1 && !path.len) continue;
    if (i == 1) wellknown(&u, &org, "oauth-protected-resource", &path);
    if (i == 2) {
      if (!stralloc_copys(&j, "")) nomem();
      wellknown(&u, &org, "oauth-protected-resource", &j);
    }
    found = getjson(u.s, &j) && jstr(j.s, j.len, "authorization_servers.0",
        &issuer);
  }
  if (found && !scope.len)
    for (i = 0; ; ++i) {
      if (!stralloc_copys(&tmp, "scopes_supported.") ||
          !stralloc_catulong(&tmp, i) || !stralloc_0(&tmp))
        nomem();
      if (!json_get(j.s, j.len, tmp.s, &v, &vn) || *v != '"') break;
      if (scope.len && !stralloc_cats(&scope, " ")) nomem();
      if (!json_unstr(&scope, v, vn)) nomem();
    }
  if (!found && !stralloc_copy(&issuer, &org)) nomem();  /* older servers */
  if (!stralloc_0(&scope) || !stralloc_0(&issuer)) nomem();
  --scope.len;
  --issuer.len;

  /* 3: the authorization server's metadata */
  if (!origin(issuer.s, &org)) {
    if (!stralloc_copys(&why, "the authorization server is not at an http "
        "or https URL"))
      nomem();
    goto fail;
  }
  pathof(issuer.s, org.len, &path);
  found = 0;
  for (i = 0; i < 3 && !found; ++i) {
    if (i == 0) wellknown(&u, &org, "oauth-authorization-server", &path);
    if (i == 1) wellknown(&u, &org, "openid-configuration", &path);
    if (i == 2) {
      if (!path.len) continue;
      if (!stralloc_copy(&u, &issuer) || !stralloc_cats(&u, "/.well-known/"
          "openid-configuration") || !stralloc_0(&u))
        nomem();
    }
    found = getjson(u.s, &j) && jstr(j.s, j.len, "authorization_endpoint",
        &authep) && jstr(j.s, j.len, "token_endpoint", &c.tokenep);
  }
  if (found) {
    if (!jstr(j.s, j.len, "registration_endpoint", &regep) &&
        !stralloc_copys(&regep, ""))
      nomem();
    if (json_get(j.s, j.len, "code_challenge_methods_supported", &v, &vn) &&
        !has(v, vn, "\"S256\"")) {
      if (!stralloc_copys(&why, "the authorization server does not do PKCE "
          "with S256, so kraft will not log in to it"))
        nomem();
      goto fail;
    }
  } else {
    /* the defaults of MCP's first authorization */
    if (!stralloc_copy(&authep, &org) || !stralloc_cats(&authep, "/authorize") ||
        !stralloc_0(&authep) || !stralloc_copy(&c.tokenep, &org) ||
        !stralloc_cats(&c.tokenep, "/token") || !stralloc_0(&c.tokenep) ||
        !stralloc_copy(&regep, &org) || !stralloc_cats(&regep, "/register") ||
        !stralloc_0(&regep))
      nomem();
    authep.len--;
    c.tokenep.len--;
    regep.len--;
  }
  if (!stralloc_0(&regep)) nomem();
  --regep.len;

  /* 4: where the browser comes back to */
  if ((ls = socket(AF_INET, SOCK_STREAM, 0)) == -1) goto sys;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  fcntl(ls, F_SETFD, FD_CLOEXEC);
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = htons(mcpcid[k] ? LOOPBACKPORT : 0);
  if (bind(ls, (struct sockaddr *)&sa, sizeof sa) == -1 || listen(ls, 4) == -1 ||
      getsockname(ls, (struct sockaddr *)&sa, &sl) == -1)
    goto sys;
  if (!stralloc_copys(&redirect, "http://127.0.0.1:") ||
      !stralloc_catulong(&redirect, ntohs(sa.sin_port)) ||
      !stralloc_cats(&redirect, "/callback") || !stralloc_0(&redirect))
    nomem();
  --redirect.len;

  /* 5: who kraft is to it */
  if (mcpcid[k]) {
    if (!stralloc_copys(&c.clientid, mcpcid[k]) ||
        !stralloc_copys(&c.secret, mcpcsecret[k] ? mcpcsecret[k] : "") ||
        !stralloc_copys(&c.method, mcpcsecret[k] ? "client_secret_basic" : "none"))
      nomem();
  } else if (!regep.len) {
    if (!stralloc_copys(&why, "the authorization server has no way to "
        "register kraft; register it by hand and see mcp_client in kraft(1)"))
      nomem();
    goto fail;
  } else if (!regist(&regep, &redirect, &c, &why))
    goto fail;
  if (!stralloc_0(&c.clientid) || !stralloc_0(&c.secret) ||
      !stralloc_0(&c.method))
    nomem();
  --c.clientid.len;
  --c.secret.len;
  --c.method.len;

  /* 6: the address to log in at */
  if (!oauth_pkce(&verifier, &challenge) || !oauth_pkce(&state, 0)) nomem();
  if (!stralloc_copy(&url, &authep) ||
      !stralloc_cats(&url, byte_chr(authep.s, authep.len, '?') < authep.len ?
      "&" : "?") || !stralloc_cats(&url, "response_type=code&client_id=") ||
      !url_enc(&url, c.clientid.s, c.clientid.len) ||
      !stralloc_cats(&url, "&redirect_uri=") ||
      !url_enc(&url, redirect.s, redirect.len) ||
      !stralloc_cats(&url, "&code_challenge=") || !stralloc_cat(&url, &challenge) ||
      !stralloc_cats(&url, "&code_challenge_method=S256&state=") ||
      !stralloc_cat(&url, &state) || !stralloc_cats(&url, "&resource=") ||
      !url_enc(&url, m->cmd, str_len(m->cmd)) ||
      (scope.len && (!stralloc_cats(&url, "&scope=") ||
      !url_enc(&url, scope.s, scope.len))) || !stralloc_0(&url))
    nomem();
  nl();
  styled(BOLD, "To log in to MCP server ");
  styled(BOLD, m->name);
  styled(BOLD, ", open this address:");
  outs("\n\n  ");
  outs(url.s);
  outs("\n\n");
  sgr(DIM);
  outs("(waiting; if your browser is on another machine, paste here the "
      "address it ends up at, even if the page will not load)");
  sgr(0);
  outs("\n");
  flush();
  openurl(url.s);
  if (!await(ls, &state, &code, &why)) goto fail;

  /* 7: the code for tokens */
  if (!stralloc_copys(&form, "grant_type=authorization_code&code=") ||
      !url_enc(&form, code.s, code.len) ||
      !stralloc_cats(&form, "&redirect_uri=") ||
      !url_enc(&form, redirect.s, redirect.len) ||
      !stralloc_cats(&form, "&code_verifier=") || !stralloc_cat(&form, &verifier) ||
      !stralloc_cats(&form, "&resource=") ||
      !url_enc(&form, m->cmd, str_len(m->cmd)))
    nomem();
  if (!mcptokenreq(&c, &form, &why)) goto fail;
  if (!stralloc_copys(&c.url, m->cmd) || !stralloc_copy(&c.issuer, &issuer))
    nomem();
  mcpcredsave(k, &c);
  if (!stralloc_copy(&m->token, &c.access)) nomem();
  sgr(GREEN);
  outs("(logged in to ");
  outs(m->name);
  outs(")");
  sgr(0);
  outs("\n");
  ok = 1;
  goto out;
sys:
  if (!stralloc_copys(&why, "unable to listen for the browser: ") ||
      !stralloc_cats(&why, strerror(errno)))
    nomem();
fail:
  if (!stralloc_0(&why)) nomem();
  kwarn("MCP server ", m->name, ": the login did not work");
  sgr(DIM);
  outs("  (");
  outs(why.s);
  outs(")\n");
  sgr(0);
out:
  if (ls != -1) close(ls);
  flush();
  return ok;
}
