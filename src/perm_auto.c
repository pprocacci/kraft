#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "byte.h"
#include "perm.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

/*
 * Auto mode: decides, without asking, whether a tool call that would
 * normally need approval is safe enough to allow.  The rules are
 * deliberately conservative; anything they do not clearly allow goes
 * back to the user.
 *
 * Files: write_file and edit_file may change files inside the working
 * directory (after resolving symbolic links and ..), but not in .git.
 *
 * Commands: bash may run a command when every simple command in it is
 * on the list below (or is ./something built in the project), and it
 * uses no command or process substitution, no here-documents, and no
 * redirection except to /dev/null or between descriptors.
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

static int
no(stralloc *why, const char *a, const char *b)
{
  stralloc_copys(why, a);
  if (b) stralloc_cats(why, b);
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
dotdot(const char *s)
{
  for (; *s; ++s)
    if (str_start(s, "..")) return 1;
  return 0;
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

/* one simple command, as words */
static int
simple(char **w, size_t n, stralloc *why)
{
  const char *p, *b;
  size_t i;

  while (n && assignment(*w)) { ++w; --n; }
  if (!n) return 1;
  p = w[0];
  if (*p == '$') return no(why, "the command name is a variable", 0);
  if (byte_chr(p, str_len(p), '/') < str_len(p)) {
    if (str_start(p, "./") && !dotdot(p)) return 1;  /* built here */
    b = p + str_len(p);
    while (b > p && b[-1] != '/') --b;
    if (b - p == 5 && str_start(p, "/bin/")) p = b;
    else if (b - p == 9 && str_start(p, "/usr/bin/")) p = b;
    else if (b - p == 15 && str_start(p, "/usr/local/bin/")) p = b;
    else return no(why, p, " is outside the project");
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
      if (byte_chr(w[i], str_len(w[i]), '=') < str_len(w[i]))
        return no(why, "sysctl setting a value", 0);
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
      b = 0;
      if (!str_diff(w[i], "-o") && i + 1 < n) b = w[i + 1];
      else if (str_start(w[i], "-o")) b = w[i] + 2;
      if (b && (*b == '/' || dotdot(b)))
        return no(why, "output outside the project: ", b);
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
segment(stralloc *sa, size_t nw, stralloc *why)
{
  char *v[MAXWORDS];
  size_t i, off = 0;

  if (nw > MAXWORDS) return no(why, "the command is too long to check", 0);
  for (i = 0; i < nw; ++i) {
    v[i] = sa->s + off;
    off += str_len(v[i]) + 1;
  }
  return simple(v, nw, why);
}

/* the target of a redirection: only /dev/null and &N are allowed */
static int
redirect(const char *s, size_t *ip, int out, stralloc *why)
{
  size_t i = *ip, e;

  if (s[i] == s[i - 1]) {
    if (!out) return no(why, "a here-document", 0);
    ++i;  /* >> */
  }
  if (s[i] == '&') {
    ++i;
    while (s[i] >= '0' && s[i] <= '9') ++i;
    if (s[i] == '-') ++i;
    *ip = i;
    return 1;
  }
  while (s[i] == ' ' || s[i] == '\t') ++i;
  for (e = i; s[e] && s[e] != ' ' && s[e] != '\t' && s[e] != ';' &&
      s[e] != '&' && s[e] != '|' && s[e] != '\n' && s[e] != '>' &&
      s[e] != '<'; ++e)
    if (s[e] == '\'' || s[e] == '"' || s[e] == '$' || s[e] == '`' ||
        s[e] == '\\')
      return no(why, "a redirection the rules cannot read", 0);
  if (out && !(e - i == 9 && !byte_diff(s + i, 9, "/dev/null")))
    return no(why, "the command writes to a file", 0);
  *ip = e;
  return 1;
}

static int
command(const char *s, stralloc *why)
{
  stralloc w = STRALLOC_ZERO;
  size_t i, start = 0, nw = 0, k;
  int q = 0, inword = 0, ok = 0;
  char c;

#define ADD(ch) do { if (!stralloc_append(&w, (ch))) goto out; inword = 1; } \
    while (0)
#define END() do { if (inword) { if (!stralloc_0(&w)) goto out; ++nw; \
    start = w.len; inword = 0; } } while (0)

  for (i = 0;; ++i) {
    c = s[i];
    if (q == '\'') {
      if (!c) { no(why, "an unterminated quote", 0); goto out; }
      if (c == '\'') q = 0; else ADD(c);
      continue;
    }
    if (q == '"') {
      if (!c) { no(why, "an unterminated quote", 0); goto out; }
      if (c == '"') { q = 0; continue; }
      if (c == '`' || (c == '$' && s[i + 1] == '(')) {
        no(why, "command substitution", 0);
        goto out;
      }
      if (c == '\\' && s[i + 1]) c = s[++i];
      ADD(c);
      continue;
    }
    if (!c || c == ';' || c == '\n' || c == '&' || c == '|') {
      END();
      if (!segment(&w, nw, why)) goto out;
      if (!stralloc_copys(&w, "")) goto out;
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
      no(why, "command substitution", 0);
      goto out;
    }
    if (c == '>' || c == '<') {
      if (s[i + 1] == '(') { no(why, "process substitution", 0); goto out; }
      /* a file descriptor number in front belongs to the redirection */
      for (k = start; k < w.len && w.s[k] >= '0' && w.s[k] <= '9'; ++k) ;
      if (inword && k == w.len) { w.len = start; inword = 0; }
      END();
      ++i;
      if (!redirect(s, &i, c == '>', why)) goto out;
      --i;
      continue;
    }
    ADD(c);
  }
  ok = 1;
out:
  stralloc_free(&w);
  return ok;
#undef ADD
#undef END
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
    no(why, t.s, " is outside the working directory");
    goto out;
  }
  for (k = cl; t.s[k]; ++k)
    if (str_start(t.s + k, "/.git") && (!t.s[k + 5] || t.s[k + 5] == '/')) {
      no(why, "the file is inside .git", 0);
      goto out;
    }
  ok = 1;
out:
  stralloc_free(&t);
  return ok;
}

/*
 * returns 1 if auto mode allows tool name with input in, 0 if the
 * user should be asked; in that case why says what the rules
 * objected to.
 */
int
perm_auto(const char *name, const char *in, size_t n, stralloc *why)
{
  stralloc a = STRALLOC_ZERO;
  int ok = 0;

  if (!str_diff(name, "write_file") || !str_diff(name, "edit_file")) {
    if (!tool_arg(in, n, "path", &a)) no(why, "no path given", 0);
    else ok = inside(a.s, why);
  } else if (!str_diff(name, "bash")) {
    if (!tool_arg(in, n, "command", &a)) no(why, "no command given", 0);
    else if (byte_chr(a.s, a.len, '\0') < a.len)
      no(why, "the command contains a NUL byte", 0);
    else ok = command(a.s, why);
  } else
    no(why, name, " is not covered by auto mode");
  stralloc_free(&a);
  return ok;
}
