#include "agent.h"

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

/*
 * works out where requests go, and with what headers: the URL's path
 * and host, and the API key, which then leaves the environment.
 */
void
setconnection(void)
{
  const char *key;
  size_t i;

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
  pdfok = api->render == anth_render && !str_diff(url.host.s, "api.anthropic.com");
}
