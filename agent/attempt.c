#include "agent.h"

static void tally(int ismain);
static void slurp(void);
static int toolong(const char *s);
static void keeptext(struct conv *c);
static void mdreset(void);

    /* the URL path before /v1, headers for GETs */

/* adds the last request's usage to the session's */
static void
tally(int ismain)
{
  total.in += reply.u.in;
  total.cread += reply.u.cread;
  total.cwrite += reply.u.cwrite;
  total.out += reply.u.out;
  ++requests;
  if (ismain) {
    lastmain = reply.u;
    pending = 0;
  }
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

/* does an error message say the prompt is too long for the model? */
static int
toolong(const char *s)
{
  static const char *const says[] = {
    "too long", "too large", "context length", "context_length",
    "context window", "maximum context", "exceeds the context", 0
  };
  size_t i;

  for (; *s; ++s)
    for (i = 0; says[i]; ++i)
      if (str_casestart(s, says[i])) return 1;
  return 0;
}

/*
 * after an interrupted reply, keeps only its text: a tool call or a
 * thinking block cut off part way would make the conversation invalid.
 */
static void
keeptext(struct conv *c)
{
  struct msg *m;
  struct block *b;
  size_t i, j;

  if (!c->n || c->m[c->n - 1].role != ROLE_ASSISTANT) return;
  m = &c->m[c->n - 1];
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
  if (!j) conv_drop(c);
}

   /* when the request went out */

/* ends the reply's Markdown: whatever it held back, and plain text again */
static void
mdreset(void)
{
  if (!color) return;
  if (!stralloc_copys(&mdout, "") || !md_end(&md, &mdout)) nomem();
  raw(mdout.s, mdout.len);
}

/*
 * sends conversation c, with system prompt sysp and tools, to model
 * mdl, and streams the reply into a new assistant message, printing
 * it as it arrives if show is set.  returns 1 on success.  on failure
 * the partial reply is dropped, so the conversation stays valid, and
 * 0 is returned.  if the user interrupts, whatever text arrived is
 * kept and -1 is returned.
 */
int
attempt(struct conv *c, const char *sysp, const struct tool *tools,
    const char *mdl, unsigned long mx, int show)
{
  struct req q;
  struct msg *m;
  size_t i;
  int fd, r;

  q.model = mdl;
  q.max = mx;
  q.system = sysp;
  q.tools = tools;
  q.conv = c;
  q.effort = effortnow ? effortnow : effort;
  /* -t on the Anthropic API: ask for reasoning there is something to show of */
  q.summarize = showthink && pdfok && show;
  if (!api->render(&body, &q)) nomem();
  if (intr) return -1;

  fd = tcp_connect(url.host.s, url.port.s, CONNECTSECS);
  if (fd == -1) {
    if (intr) return -1;
    kwarnsys("unable to connect to ", base);
    again = 1;
    goto fail;
  }
  if (url.tls && (fd = tls_connect(fd, url.host.s, &tlswhy)) == -1) {
    if (intr) return -1;
    if (!tlswhy.len) kwarnsys("unable to start TLS with ", base);
    else {
      if (!stralloc_copys(&tmp, base) || !stralloc_cats(&tmp, ": ") ||
          !stralloc_cats(&tmp, tlswhy.s) || !stralloc_0(&tmp))
        nomem();
      kwarn("unable to start TLS with ", tmp.s, 0);
    }
    goto fail;
  }
  curfd = fd;
  if (intr) goto stopped;  /* ^C came before the handler knew fd */
  if (!http_request(fd, "POST", host.s, path.s, hdrs.s, body.s, body.len)) {
    if (intr) goto stopped;
    kwarnsys("unable to send request to ", base);
    again = 1;
    goto failfd;
  }
  clock_gettime(CLOCK_MONOTONIC, &asked);
  if (show && color && last == '\n') spin(fd);
  r = http_response(&http, fd);
  if (r <= 0) {
    if (intr) goto stopped;
    if (r < 0) kwarnsys("unable to read response from ", base);
    else kwarn("malformed response from ", base, 0);
    again = 1;
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
    if (intr) { kwarn("interrupted", 0, 0); return -1; }
    if ((http.status == 400 || http.status == 413) && toolong(body.s)) {
      overflow = 1;  /* the caller may compact and try again */
      return 0;
    }
    kwarn("HTTP ", tmp.s, body.s);
    status = http.status < 500 ? 100 : 111;
    if (http.status == 429 || http.status == 500 || http.status == 502 ||
        http.status == 503 || http.status == 504 || http.status == 529) {
      again = 1;  /* busy or broken for now; try again later */
      waitsecs = http.retry;
    }
    return 0;
  }

  if (!(m = conv_msg(c, ROLE_ASSISTANT))) nomem();
  sse.p = sse.n = 0;
  sse.eof = 0;
  reply.stop = STOP_NONE;
  reply.retry = 0;
  reply.u = nousage;
  for (;;) {
    /* nothing to show yet: say so while waiting */
    if (show && color && last == '\n' && sse.p == sse.n &&
        http.b.p == http.b.n)
      spin(fd);
    r = sse_get(&sse, &http, &event, &data);
    if (r < 0) {
      if (intr) goto cut;
      kwarnsys("unable to read stream from ", base);
      again = 1;
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
    if (show && showthink && reply.think.len) {
      if (color) buffer_puts(buffer_2, "\033[2;3m");  /* dim italic */
      buffer_put(buffer_2, reply.think.s, reply.think.len);
      if (color) buffer_puts(buffer_2, "\033[0m");
      buffer_flush(buffer_2);
    }
    if (show && reply.text.len) {
      /* a server tool's "\n* name(...)" line needs no blank line before it */
      i = last == '\n' && reply.text.len > 2 &&
          !byte_diff(reply.text.s, 3, "\n* ");
      if (color) {
        if (!stralloc_copys(&mdout, "") ||
            !md_put(&md, &mdout, reply.text.s + i, reply.text.len - i))
          nomem();
        raw(mdout.s, mdout.len);
        if (reply.text.len > i) last = reply.text.s[reply.text.len - 1];
      } else
        out(reply.text.s + i, reply.text.len - i);
      flush();
    }
    if (r == API_ERROR) {
      if (!stralloc_0(&reply.err)) nomem();
      kwarn("API error: ", reply.err.s, 0);
      again = reply.retry;
      goto drop;
    }
    if (r == API_DONE) break;
  }
  curfd = -1;
  close(fd);
  tally(c == &conv);
  if (show) {
    mdreset();
    nl();
  }
  flush();
  return 1;

cut:
  tally(c == &conv);
  keeptext(c);
  if (show) mdreset();
stopped:
  curfd = -1;
  close(fd);
  kwarn("interrupted", 0, 0);
  return -1;
drop:
  if (show) mdreset();
  conv_drop(c);
failfd:
  curfd = -1;
  close(fd);
fail:
  status = 111;
  return 0;
}
