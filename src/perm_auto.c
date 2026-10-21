#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "byte.h"
#include "path.h"
#include "perm.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

/*
 * Auto mode's rules: decide, without asking, whether a tool call that
 * would normally need approval is safe enough to allow.  They are
 * deliberately conservative.  Besides "allow", they can say "unsure"
 * (a model may then judge) or "ask" (risky enough that only the user
 * may allow it).
 *
 * Files: write_file and edit_file may change files inside the working
 * directory (after resolving symbolic links and ..), but not in .git.
 * Anything outside, or in .git, is for the user.
 *
 * Commands: bash may run a command when every simple command in it is
 * on the list below (or is ./something built in the project), and it
 * uses no command or process substitution, no here-documents, and no
 * redirection except to /dev/null or between descriptors.  Running as
 * another user, rm -r or -f, git push, piping into a shell and a few
 * destructive system tools are for the user, wherever they appear.
 */

#define MAXWORDS 256

static const char *const safe[] = {
  /* looking around */
  "ls", "cat", "head", "tail", "wc", "file", "stat", "du", "df", "pwd",
  "which", "whereis", "uname", "date", "id", "whoami", "hostname",
  "readlink", "realpath", "basename", "dirname", "tree", "ps",
  /* text */
  "grep", "egrep", "fgrep", "rg", "sort", "uniq", "cut", "tr", "nl",
  "diff", "cmp", "comm", "od", "hexdump", "strings", "column", "fold",
  "rev", "md5", "sha1", "sha256", "cksum", "mandoc",
  /* shell basics */
  "echo", "printf", "true", "false", "test", "[", "expr", "seq",
  "sleep", "cd",
  /* building, in the project */
  "make", "cc", "c++", "clang", "clang++", "gcc", "g++", "ar", "nm",
  "objdump", "size", "ldd",
  /* with extra checks below */
  "find", "sed", "git", "sysctl", "pkg",
  0
};

static const char *const gitsafe[] = {
  "status", "diff", "log", "show", "rev-parse", "ls-files", "blame",
  "grep", "describe", "shortlog", "branch", 0
};

static const char *const asuser[] = { "sudo", "su", "doas", "pkexec", 0 };

static const char *const drastic[] = {
  "dd", "mkfs", "newfs", "gpart", "fdisk", "shutdown", "reboot", "halt",
  "poweroff", "eval", 0
};

static const char *const shells[] = {
  "sh", "bash", "zsh", "ksh", "csh", "tcsh", "dash", "fish", 0
};

static int hard;      /* found something only the user may allow */
static int reasoned;  /* why has been set */

/* not allowed; why says so, unless an earlier reason stands */
static int
no(stralloc *why, const char *a, const char *b)
{
  if (!reasoned) {
    stralloc_copys(why, a);
    if (b) stralloc_cats(why, b);
    reasoned = 1;
  }
  return 0;
}

/* not allowed, and not for a model to overrule either */
static int
never(stralloc *why, const char *a, const char *b)
{
  if (!hard) {
    stralloc_copys(why, a);
    if (b) stralloc_cats(why, b);
  }
  reasoned = hard = 1;
  return 0;
}

static int
among(const char *w, const char *const *list)
{
  for (; *list; ++list)
    if (!str_diff(w, *list)) return 1;
  return 0;
}

static int
mentions(const char *s, const char *t)
{
  for (; *s; ++s)
    if (str_start(s, t)) return 1;
  return 0;
}

static int
dotdot(const char *s)
{
  for (; *s; ++s)
    if (str_start(s, "..")) return 1;
  return 0;
}

static int
has(const char *s, char c)
{
  return byte_chr(s, str_len(s), c) < str_len(s);
}

static int
assignment(const char *w)
{
  const char *s = w;

  if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_'))
    return 0;
  while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
      (*s >= '0' && *s <= '9') || *s == '_')
    ++s;
  return *s == '=';
}

/* the risky things, checked wherever they appear */
static void
risky(char **w, size_t n, const char *base, int piped, stralloc *why)
{
  size_t i;

  if (among(base, asuser)) never(why, base, " runs a command as another user");
  else if (among(base, drastic)) never(why, base, " is never allowed automatically");
  else if (piped && among(base, shells)) never(why, "piping into ", base);
  else if (!str_diff(base, "rm")) {
    for (i = 1; i < n; ++i)
      if (w[i][0] == '-' && (has(w[i], 'r') || has(w[i], 'R') ||
          has(w[i], 'f')))
        never(why, "rm ", w[i]);
  } else if (!str_diff(base, "chmod") || !str_diff(base, "chown") ||
      !str_diff(base, "chgrp") || !str_diff(base, "chflags")) {
    for (i = 1; i < n; ++i)
      if (w[i][0] == '-' && has(w[i], 'R')) never(why, base, " -R");
  } else if (!str_diff(base, "git")) {
    for (i = 1; i < n && w[i][0] == '-'; ++i) ;
    if (i < n && (!str_diff(w[i], "push") || !str_diff(w[i], "clean")))
      never(why, "git ", w[i]);
  }
}

/* one simple command, as words; 1 if allowed */
static int
simple(char **w, size_t n, int piped, stralloc *why)
{
  const char *p, *base;
  size_t i;

  while (n && assignment(*w)) { ++w; --n; }
  if (!n) return 1;
  p = w[0];
  for (base = p + str_len(p); base > p && base[-1] != '/'; --base) ;
  risky(w, n, base, piped, why);
  if (hard) return 0;

  if (*p == '$') return no(why, "the command name is a variable", 0);
  if (has(p, '/')) {
    if (str_start(p, "./") && !dotdot(p)) return 1;  /* built here */
    if (!((base - p == 5 && str_start(p, "/bin/")) ||
        (base - p == 9 && str_start(p, "/usr/bin/")) ||
        (base - p == 15 && str_start(p, "/usr/local/bin/"))))
      return no(why, p, " is outside the project");
    p = base;
  }
  if (!among(p, safe)) return no(why, p, " is not on the auto-mode list");

  if (!str_diff(p, "find")) {
    for (i = 1; i < n; ++i)
      if (str_start(w[i], "-exec") || str_start(w[i], "-ok") ||
          !str_diff(w[i], "-delete") || str_start(w[i], "-fprint") ||
          !str_diff(w[i], "-fls"))
        return no(why, "find with ", w[i]);
  } else if (!str_diff(p, "sed")) {
    for (i = 1; i < n; ++i)
      if (str_start(w[i], "-i") || str_start(w[i], "--in-place"))
        return no(why, "sed editing in place", 0);
  } else if (!str_diff(p, "git")) {
    for (i = 1; i < n && w[i][0] == '-'; ++i)
      if (!str_diff(w[i], "-C") || !str_diff(w[i], "-c"))
        return no(why, "git ", w[i]);
    if (i == n) return 1;
    if (!among(w[i], gitsafe)) return no(why, "git ", w[i]);
    if (!str_diff(w[i], "branch"))
      for (++i; i < n; ++i)
        if (str_diff(w[i], "-a") && str_diff(w[i], "-v") &&
            str_diff(w[i], "-vv") && str_diff(w[i], "-r") &&
            str_diff(w[i], "--list"))
          return no(why, "git branch ", w[i]);
  } else if (!str_diff(p, "sysctl")) {
    for (i = 1; i < n; ++i)
      if (has(w[i], '=')) return no(why, "sysctl setting a value", 0);
  } else if (!str_diff(p, "pkg")) {
    if (n < 2 || (str_diff(w[1], "info") && str_diff(w[1], "search") &&
        str_diff(w[1], "query") && str_diff(w[1], "which")))
      return no(why, "pkg ", n < 2 ? "with no subcommand" : w[1]);
  } else if (!str_diff(p, "make")) {
    for (i = 1; i < n; ++i)
      if (str_start(w[i], "install") || str_start(w[i], "deinstall") ||
          str_start(w[i], "reinstall") || str_start(w[i], "uninstall") ||
          str_start(w[i], "DESTDIR=") || str_start(w[i], "PREFIX="))
        return no(why, "make ", w[i]);
  } else if (!str_diff(p, "cc") || !str_diff(p, "c++") ||
      !str_diff(p, "clang") || !str_diff(p, "clang++") ||
      !str_diff(p, "gcc") || !str_diff(p, "g++")) {
    for (i = 1; i < n; ++i) {
      base = 0;
      if (!str_diff(w[i], "-o") && i + 1 < n) base = w[i + 1];
      else if (str_start(w[i], "-o")) base = w[i] + 2;
      if (base && (*base == '/' || dotdot(base)))
        return no(why, "output outside the project: ", base);
    }
  } else if (!str_diff(p, "ar")) {
    for (i = 1; i < n; ++i)
      if (w[i][0] == '/' || dotdot(w[i]))
        return no(why, "ar on a file outside the project: ", w[i]);
  }
  return 1;
}

/* splits words (NUL-separated in sa) into v and checks them */
static int
segment(stralloc *sa, size_t nw, int piped, stralloc *why)
{
  char *v[MAXWORDS];
  size_t i, off = 0;

  if (nw > MAXWORDS) return no(why, "the command is too long to check", 0);
  for (i = 0; i < nw; ++i) {
    v[i] = sa->s + off;
    off += str_len(v[i]) + 1;
  }
  return simple(v, nw, piped, why);
}

/*
 * the target of a redirection starting at s[*ip], just after the > or
 * <; only /dev/null and &N are allowed for output.  always moves *ip
 * past it.
 */
static int
redirect(const char *s, size_t *ip, int out, stralloc *why)
{
  size_t i = *ip, e;
  int ok = 1;

  if (s[i] == s[i - 1]) {
    ++i;  /* >> or << */
    if (!out) ok = no(why, "a here-document", 0);
  }
  if (s[i] == '&') {
    ++i;
    while (s[i] >= '0' && s[i] <= '9') ++i;
    if (s[i] == '-') ++i;
    *ip = i;
    return ok;
  }
  while (s[i] == ' ' || s[i] == '\t') ++i;
  for (e = i; s[e] && s[e] != ' ' && s[e] != '\t' && s[e] != ';' &&
      s[e] != '&' && s[e] != '|' && s[e] != '\n' && s[e] != '>' &&
      s[e] != '<'; ++e)
    if (s[e] == '\'' || s[e] == '"' || s[e] == '$' || s[e] == '`' ||
        s[e] == '\\')
      ok = no(why, "a redirection the rules cannot read", 0);
  if (out && !(e - i == 9 && !byte_diff(s + i, 9, "/dev/null")))
    ok = no(why, "the command writes to a file", 0);
  *ip = e;
  return ok;
}

/*
 * splits a command into simple commands and checks each.  carries on
 * past the first objection so that a risky part later on is still
 * seen.  returns 1 if everything is allowed.
 */
static int
command(const char *s, stralloc *why)
{
  stralloc w = STRALLOC_ZERO;
  size_t i, start = 0, nw = 0, k;
  int q = 0, inword = 0, ok = 1, piped = 0, nextpiped;
  char c;

#define ADD(ch) do { if (!stralloc_append(&w, (ch))) goto nomem; \
    inword = 1; } while (0)
#define END() do { if (inword) { if (!stralloc_0(&w)) goto nomem; ++nw; \
    start = w.len; inword = 0; } } while (0)

  for (i = 0;; ++i) {
    c = s[i];
    if (q == '\'') {
      if (!c) { ok = no(why, "an unterminated quote", 0); c = 0; q = 0; }
      else { if (c == '\'') q = 0; else ADD(c); continue; }
    } else if (q == '"') {
      if (!c) { ok = no(why, "an unterminated quote", 0); q = 0; }
      else {
        if (c == '"') { q = 0; continue; }
        if (c == '`' || (c == '$' && s[i + 1] == '('))
          ok = no(why, "command substitution", 0);
        if (c == '\\' && s[i + 1]) c = s[++i];
        ADD(c);
        continue;
      }
    }
    if (!c || c == ';' || c == '\n' || c == '&' || c == '|') {
      END();
      nextpiped = c == '|' && s[i + 1] != '|';
      if (!segment(&w, nw, piped, why)) ok = 0;
      piped = nextpiped;
      if (!stralloc_copys(&w, "")) goto nomem;
      nw = 0;
      start = 0;
      if (!c) break;
      if ((c == '&' || c == '|') && s[i + 1] == c) ++i;
      else if (c == '|' && s[i + 1] == '&') ++i;
      continue;
    }
    if (c == ' ' || c == '\t') { END(); continue; }
    if (c == '\\') {
      if (s[i + 1]) ADD(s[++i]);
      continue;
    }
    if (c == '\'' || c == '"') { q = c; inword = 1; continue; }
    if (c == '`' || (c == '$' && s[i + 1] == '(')) {
      ok = no(why, "command substitution", 0);
      ADD(c);
      continue;
    }
    if (c == '>' || c == '<') {
      if (s[i + 1] == '(') {
        ok = no(why, "process substitution", 0);
        ADD(c);
        continue;
      }
      /* a file descriptor number in front belongs to the redirection */
      for (k = start; k < w.len && w.s[k] >= '0' && w.s[k] <= '9'; ++k) ;
      if (inword && k == w.len) { w.len = start; inword = 0; }
      END();
      ++i;
      if (!redirect(s, &i, c == '>', why)) ok = 0;
      --i;
      continue;
    }
    ADD(c);
  }
  stralloc_free(&w);
  return ok;
nomem:
  stralloc_free(&w);
  return no(why, "out of memory", 0);
#undef ADD
#undef END
}

/*
 * inside $( ) or backquotes the word splitter does not see command
 * names, so look for the never-allowed programs among all the tokens
 */
static void
substituted(const char *s, stralloc *why)
{
  char tok[64];
  const char *base;
  size_t n;

  while (*s) {
    for (n = 0; *s && ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
        (*s >= '0' && *s <= '9') || *s == '_' || *s == '.' || *s == '/' ||
        *s == '-' || *s == '+'); ++s)
      if (n < sizeof tok - 1) tok[n++] = *s;
    tok[n] = 0;
    for (base = tok + n; base > tok && base[-1] != '/'; --base) ;
    if (among(base, asuser) || among(base, drastic))
      never(why, base, " inside a command substitution");
    if (*s) ++s;
  }
}

/* 1 if path is (or would be) inside the working directory, not in .git */
static int
inside(const char *path, stralloc *why)
{
  char cwd[PATH_MAX], real[PATH_MAX];
  struct stat st;
  stralloc t = STRALLOC_ZERO;
  size_t n, cl, k;
  const char *base;
  int ok = 0;

  if (!realpath(".", cwd)) return no(why, "the working directory is unknown", 0);
  cl = str_len(cwd);
  if (realpath(path, real)) {
    if (stat(real, &st) == 0 && S_ISDIR(st.st_mode)) {
      no(why, path, " is a directory");
      goto out;
    }
    if (!stralloc_copys(&t, real)) goto out;
  } else {
    /* a new file: resolve its directory instead */
    n = str_len(path);
    while (n && path[n - 1] != '/') --n;
    base = path + n;
    if (!*base || !str_diff(base, ".") || !str_diff(base, "..")) {
      no(why, "the path does not name a file", 0);
      goto out;
    }
    if (!stralloc_copyb(&t, path, n ? n : 1)) goto out;
    if (!n) t.s[0] = '.';
    if (!stralloc_0(&t)) goto out;
    if (!realpath(t.s, real)) {
      no(why, "the directory does not exist: ", t.s);
      goto out;
    }
    if (!stralloc_copys(&t, real) || !stralloc_cats(&t, "/") ||
        !stralloc_cats(&t, base))
      goto out;
  }
  if (!stralloc_0(&t)) goto out;
  if (!(str_start(t.s, cwd) && (t.s[cl] == '/' || (cl == 1 && *cwd == '/')))) {
    never(why, t.s, " is outside the working directory");
    goto out;
  }
  for (k = cl; t.s[k]; ++k)
    if (str_start(t.s + k, "/.git") && (!t.s[k + 5] || t.s[k + 5] == '/')) {
      never(why, "the file is inside .git", 0);
      goto out;
    }
  ok = 1;
out:
  stralloc_free(&t);
  return ok;
}

/*
 * judges tool name with input in.  returns PERM_ALLOW, PERM_UNSURE or
 * PERM_ASK; unless it allows, why says what the rules objected to.
 */
int
perm_auto(const char *name, const char *in, size_t n, stralloc *why)
{
  stralloc a = STRALLOC_ZERO;
  int ok = 0;

  hard = reasoned = 0;
  if (!str_diff(name, "write_file") || !str_diff(name, "edit_file")) {
    if (!tool_arg(in, n, "path", &a)) no(why, "no path given", 0);
    else if (path_secret(a.s))
      never(why, "the file is one of kraft's private files", 0);
    else ok = inside(a.s, why);
  } else if (!str_diff(name, "bash")) {
    if (!tool_arg(in, n, "command", &a)) no(why, "no command given", 0);
    else if (byte_chr(a.s, a.len, '\0') < a.len)
      never(why, "the command contains a NUL byte", 0);
    else if (mentions(a.s, ".kraft"))
      never(why, "the command refers to kraft's private files", 0);
    else {
      ok = command(a.s, why);
      if (has(a.s, '`') || byte_chr(a.s, a.len, '$') < a.len)
        substituted(a.s, why);
    }
  } else
    no(why, name, " is not covered by the rules");
  stralloc_free(&a);
  if (ok && !hard) return PERM_ALLOW;
  return hard ? PERM_ASK : PERM_UNSURE;
}
