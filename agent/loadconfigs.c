#include "agent.h"

static void loadcfg(const char *fn, const stralloc *text);
static int trusted(const char *fn, const char *hash);

static void
loadcfg(const char *fn, const stralloc *text)
{
  cfgfile = sdup(fn);
  if (cfg_parse(text->s, text->len, setting, 0)) nomem();
}

/* is fn, with this hash, in ~/.kraft/trusted? */
static int
trusted(const char *fn, const char *hash)
{
  stralloc t = STRALLOC_ZERO, l = STRALLOC_ZERO;
  size_t i, e, h = str_len(hash);
  int yes = 0;

  if (!sessdir(&tmp)) return 0;  /* makes ~/.kraft if need be */
  tmp.len -= str_len("/sessions");
  if (!stralloc_cats(&tmp, "/trusted") || !stralloc_0(&tmp)) nomem();
  if (file_get(tmp.s, &t))
    for (i = 0; i < t.len && !yes; i = e + 1) {
      e = i + byte_chr(t.s + i, t.len - i, '\n');
      if (!stralloc_copyb(&l, t.s + i, e - i) || !stralloc_0(&l)) nomem();
      yes = l.len > h + 1 && !byte_diff(l.s, h, hash) && l.s[h] == ' ' &&
          !str_diff(l.s + h + 1, fn);
    }
  stralloc_free(&t);
  stralloc_free(&l);
  return yes;
}

/*
 * ~/.kraft/config, then the project's .kraft/config if the user
 * trusts it: a project from elsewhere could otherwise allow itself
 * anything.  the trust is for the file's contents, so a change to it
 * (a git pull, say) has to be trusted again.
 */
void
loadconfigs(void)
{
  stralloc f = STRALLOC_ZERO, hash = STRALLOC_ZERO;
  const char *home = env("HOME", 0);
  char cwd[PATH_MAX];
  int c;

  if (home) {
    if (!stralloc_copys(&tmp, home) || !stralloc_cats(&tmp, "/.kraft/config") ||
        !stralloc_0(&tmp))
      nomem();
    if (file_get(tmp.s, &f)) {
      if (!stralloc_copy(&hash, &tmp)) nomem();
      loadcfg(hash.s, &f);
    }
  }
  if (!getcwd(cwd, sizeof cwd)) goto out;
  if (!stralloc_copys(&projcfg, cwd) || !stralloc_cats(&projcfg, "/.kraft/config") ||
      !stralloc_0(&projcfg))
    nomem();
  if (home && !str_diff(cwd, home)) goto out;  /* that was ~/.kraft/config */
  if (!file_get(projcfg.s, &f)) goto out;
  if (!stralloc_copys(&hash, "") || !sha256_hex(&hash, f.s, f.len) ||
      !stralloc_0(&hash))
    nomem();
  if (!trusted(projcfg.s, hash.s)) {
    if (!isatty(0)) {
      kwarn("ignoring ", projcfg.s, " until you trust it (run kraft here on "
          "a terminal)");
      goto out;
    }
    styled(YELLOW, "This project has settings of its own, which could change "
        "the permission mode, allow commands to run without asking, or run "
        "commands of their own (hooks, MCP servers):");
    outs("\n");
    lines("  | ", f.s, f.len, 40);
    flush();
    c = ui_ask(color ? "  \033[1;33mUse them?\033[0m [y]es, [n]o: " :
        "  Use them? [y]es, [n]o: ");
    last = '\n';
    if (c != 'y' && c != 'a' && c != 'p') {
      outs("(ignoring them this time)\n");
      goto out;
    }
    trust(projcfg.s, hash.s);
  }
  loadcfg(projcfg.s, &f);
out:
  stralloc_free(&f);
  stralloc_free(&hash);
}
