#include <stdlib.h>
#include <unistd.h>
#include "anth.h"
#include "api.h"
#include "buffer.h"
#include "byte.h"
#include "http.h"
#include "oai.h"
#include "scan.h"
#include "sse.h"
#include "str.h"
#include "strerr.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"
#include "url.h"

#define FATAL "kraft: fatal: "
#define USAGE "kraft: usage: kraft [-t] [-a anthropic|openai] [-m model] " \
    "[-n max_tokens] [-u url] [prompt ...]"

static const struct api apis[] = {
  { "anthropic", "/v1/messages", "ANTHROPIC_API_KEY", "x-api-key: ",
    "anthropic-version: 2023-06-01\r\n", anth_body, anth_delta },
  { "openai", "/v1/chat/completions", "OPENAI_API_KEY",
    "Authorization: Bearer ", "", oai_body, oai_delta },
};

static stralloc prompt = STRALLOC_ZERO;
static stralloc body = STRALLOC_ZERO;
static stralloc hdrs = STRALLOC_ZERO;
static stralloc path = STRALLOC_ZERO;
static stralloc host = STRALLOC_ZERO;
static stralloc event = STRALLOC_ZERO;
static stralloc data = STRALLOC_ZERO;
static stralloc text = STRALLOC_ZERO;
static stralloc think = STRALLOC_ZERO;
static struct url url;
static struct http http;
static struct sse sse;

static void
nomem(void)
{
  strerr_die(111, FATAL, "out of memory", 0);
}

static const char *
env(const char *name, const char *dflt)
{
  const char *s = getenv(name);

  return (s && *s) ? s : dflt;
}

static void
readstdin(void)
{
  ssize_t r;

  for (;;) {
    if (!stralloc_readyplus(&prompt, 4096)) nomem();
    r = buffer_unixread(0, prompt.s + prompt.len, 4096);
    if (r < 0) strerr_sys(111, FATAL, "unable to read stdin", 0);
    if (r == 0) break;
    prompt.len += (size_t)r;
  }
}

/* the body of a failed response, for the error message */
static void
slurp(void)
{
  char buf[1024];
  ssize_t r;

  while (body.len < 4096 && (r = http_get(&http, buf, sizeof buf)) > 0)
    if (!stralloc_catb(&body, buf, (size_t)r)) nomem();
  if (!stralloc_0(&body)) nomem();
}

int
main(int argc, char **argv)
{
  const struct api *api = 0;
  const char *aname, *model, *base, *key;
  unsigned long max = 8192;
  int opt, fd, r, showthink = 0;
  size_t i;
  char last = '\n';

  aname = env("KRAFT_API", "anthropic");
  model = env("KRAFT_MODEL", 0);
  base = env("KRAFT_URL", "http://127.0.0.1:1234");
  while ((opt = getopt(argc, argv, "a:m:n:tu:")) != -1)
    switch (opt) {
    case 'a': aname = optarg; break;
    case 'm': model = optarg; break;
    case 'n':
      if (!*optarg || scan_ulong(optarg, &max) != str_len(optarg) || !max)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 't': showthink = 1; break;
    case 'u': base = optarg; break;
    default: strerr_die(100, USAGE, 0, 0);
    }
  argv += optind;

  for (i = 0; i < sizeof apis / sizeof apis[0]; ++i)
    if (!str_diff(aname, apis[i].name)) api = &apis[i];
  if (!api) strerr_die(100, FATAL, "unknown API: ", aname);
  if (!model) strerr_die(100, FATAL, "no model: use -m or set KRAFT_MODEL", 0);

  if (*argv) {
    for (; *argv; ++argv) {
      if (prompt.len && !stralloc_append(&prompt, ' ')) nomem();
      if (!stralloc_cats(&prompt, *argv)) nomem();
    }
  } else
    readstdin();
  if (!prompt.len) strerr_die(100, FATAL, "empty prompt", 0);

  if (!url_parse(&url, base)) strerr_die(100, FATAL, "bad URL: ", base);

  /* accept base URLs given with or without a trailing /v1 */
  i = url.path.len - 1;
  if (i >= 3 && !byte_diff(url.path.s + i - 3, 3, "/v1")) i -= 3;
  if (!stralloc_copyb(&path, url.path.s, i)) nomem();
  if (!stralloc_cats(&path, api->endpoint)) nomem();
  if (!stralloc_0(&path)) nomem();

  if (byte_chr(url.host.s, url.host.len, ':') < url.host.len - 1) {
    if (!stralloc_copys(&host, "[")) nomem();
    if (!stralloc_cats(&host, url.host.s)) nomem();
    if (!stralloc_cats(&host, "]")) nomem();
  } else if (!stralloc_copys(&host, url.host.s)) nomem();
  if (str_diff(url.port.s, url.tls ? "443" : "80")) {
    if (!stralloc_cats(&host, ":")) nomem();
    if (!stralloc_cats(&host, url.port.s)) nomem();
  }
  if (!stralloc_0(&host)) nomem();

  if (!stralloc_copys(&hdrs, "Content-Type: application/json\r\n"
      "Accept: text/event-stream\r\n"))
    nomem();
  if (!stralloc_cats(&hdrs, api->hdrs)) nomem();
  key = env("KRAFT_API_KEY", env(api->keyenv, 0));
  if (key) {
    if (!stralloc_cats(&hdrs, api->keyhdr)) nomem();
    if (!stralloc_cats(&hdrs, key)) nomem();
    if (!stralloc_cats(&hdrs, "\r\n")) nomem();
  }
  if (!stralloc_0(&hdrs)) nomem();

  if (!api->body(&body, model, max, prompt.s, prompt.len)) nomem();

  fd = tcp_connect(url.host.s, url.port.s);
  if (fd == -1) strerr_sys(111, FATAL, "unable to connect to ", base);
  if (url.tls && (fd = tls_connect(fd, url.host.s)) == -1)
    strerr_sys(111, FATAL, "unable to start TLS with ", base);
  if (!http_request(fd, "POST", host.s, path.s, hdrs.s, body.s, body.len))
    strerr_sys(111, FATAL, "unable to send request to ", base);
  r = http_response(&http, fd);
  if (r < 0) strerr_sys(111, FATAL, "unable to read response from ", base);
  if (r == 0) strerr_die(111, FATAL, "malformed response from ", base);
  if (http.status != 200) {
    stralloc st = STRALLOC_ZERO;

    if (!stralloc_catulong(&st, (unsigned long)http.status)) nomem();
    if (!stralloc_cats(&st, ": ")) nomem();
    if (!stralloc_0(&st)) nomem();
    if (!stralloc_copys(&body, "")) nomem();
    slurp();
    strerr_die(http.status < 500 ? 100 : 111, FATAL "HTTP ", st.s, body.s);
  }

  for (;;) {
    r = sse_get(&sse, &http, &event, &data);
    if (r < 0) strerr_sys(111, FATAL, "unable to read stream from ", base);
    if (r == 0) break;
    if (!stralloc_copys(&text, "") || !stralloc_copys(&think, "")) nomem();
    r = api->delta(data.s, data.len, &text, &think);
    if (r == API_ERROR) {
      if (!stralloc_0(&text)) nomem();
      strerr_die(111, FATAL, text.s, 0);
    }
    if (showthink && think.len) {
      buffer_put(buffer_2, think.s, think.len);
      buffer_flush(buffer_2);
    }
    if (text.len) {
      last = text.s[text.len - 1];
      if (buffer_put(buffer_1, text.s, text.len) == -1 ||
          buffer_flush(buffer_1) == -1)
        strerr_sys(111, FATAL, "unable to write output", 0);
    }
    if (r == API_DONE) break;
  }
  if (last != '\n') buffer_puts(buffer_1, "\n");
  if (buffer_flush(buffer_1) == -1)
    strerr_sys(111, FATAL, "unable to write output", 0);
  close(fd);
  return 0;
}
