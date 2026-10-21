#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>
#include <string.h>
#include "byte.h"
#include "path.h"
#include "proc.h"
#include "str.h"
#include "stralloc.h"
#include "tool.h"

#define MAXSHOWN 300

/* the n-th tab-separated field of line s (len n), into f, NUL-terminated */
static void
field(const char *s, size_t len, int k, stralloc *f)
{
  size_t i = 0, e;

  while (k-- > 0) {
    i += byte_chr(s + i, len - i, '\t');
    if (i < len) ++i;
  }
  e = i + byte_chr(s + i, len - i, '\t');
  if (!stralloc_copyb(f, s + i, e - i) || !stralloc_0(f)) f->len = 0;
}

/*
 * symbols: optional name (exact, or a glob), kind and path (default
 * the working directory).  finds where things are defined, with
 * Universal Ctags: "file:line kind name signature".  given a file and
 * no name, it is the file's outline.
 */
int
tool_symbols(const char *in, size_t n, stralloc *out)
{
  static stralloc tags, name, kind, dir, f0, f1, f2, f3, f4;
  char *argv[12];
  unsigned long shown = 0, more = 0;
  size_t i, e;
  int status, glob, ok = 0;

  if (!tool_arg(in, n, "name", &name)) stralloc_copys(&name, "");
  if (!tool_arg(in, n, "kind", &kind)) stralloc_copys(&kind, "");
  if (!tool_arg(in, n, "path", &dir) || !dir.len)
    if (!stralloc_copys(&dir, ".") || !stralloc_0(&dir)) goto out;
  if (path_secret(dir.s)) {
    stralloc_copys(out, "refused: ");
    stralloc_cats(out, dir.s);
    stralloc_cats(out, " is one of kraft's private files");
    goto out;
  }
  glob = name.len && (strchr(name.s, '*') || strchr(name.s, '?') ||
      strchr(name.s, '['));

  i = 0;
  argv[i++] = UCTAGS;
  argv[i++] = "-R";
  argv[i++] = "-x";
  argv[i++] = "--sort=no";
  argv[i++] = "--_xformat=%N\t%K\t%n\t%F\t%S";
  argv[i++] = "--exclude=.git";
  argv[i++] = "--exclude=.kraft";
  argv[i++] = "--exclude=node_modules";
  argv[i++] = dir.s;
  argv[i] = 0;
  if (!stralloc_copys(&tags, "") ||
      !proc_run(argv, 120, 16 * 1024 * 1024, &tags, &status)) {
    stralloc_copys(out, "unable to run ctags: ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  if (status) {
    stralloc_copys(out, "ctags failed: ");
    stralloc_catb(out, tags.s, tags.len > 500 ? 500 : tags.len);
    goto out;
  }

  stralloc_copys(out, "");
  for (i = 0; i < tags.len; i = e + 1) {
    e = i + byte_chr(tags.s + i, tags.len - i, '\n');
    field(tags.s + i, e - i, 0, &f0);  /* name */
    field(tags.s + i, e - i, 1, &f1);  /* kind */
    if (!f0.len || !f1.len) continue;
    if (name.len && (glob ? fnmatch(name.s, f0.s, 0) : str_diff(name.s, f0.s)))
      continue;
    if (kind.len && str_diff(kind.s, f1.s)) continue;
    if (shown == MAXSHOWN) { ++more; continue; }
    ++shown;
    field(tags.s + i, e - i, 2, &f2);  /* line */
    field(tags.s + i, e - i, 3, &f3);  /* file */
    field(tags.s + i, e - i, 4, &f4);  /* signature, or - */
    stralloc_cats(out, str_start(f3.s, "./") ? f3.s + 2 : f3.s);
    stralloc_cats(out, ":");
    stralloc_cats(out, f2.s);
    stralloc_cats(out, "  ");
    stralloc_cats(out, f1.s);
    stralloc_cats(out, "  ");
    stralloc_cats(out, f0.s);
    if (str_diff(f4.s, "-")) stralloc_cats(out, f4.s);
    stralloc_append(out, '\n');
  }
  if (!shown) stralloc_cats(out, "no symbols matched");
  if (more) {
    stralloc_cats(out, "[");
    stralloc_catulong(out, more);
    stralloc_cats(out, " more; narrow it by name, kind or path]");
  }
  ok = 1;
out:
  return ok;
}
