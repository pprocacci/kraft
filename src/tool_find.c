#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <string.h>
#include "str.h"
#include "stralloc.h"
#include "tool.h"

#define MAXHITS 500

/* find_files: pattern, optional path */
int
tool_find(const char *in, size_t n, stralloc *out)
{
  stralloc pat = STRALLOC_ZERO, dir = STRALLOC_ZERO;
  char *roots[2];
  FTS *fts;
  FTSENT *e;
  const char *rel;
  unsigned long hits = 0, more = 0;
  size_t dl;
  int slash, ok = 0;

  if (!tool_arg(in, n, "pattern", &pat)) {
    stralloc_copys(out, "pattern is required");
    return 0;
  }
  if (!tool_arg(in, n, "path", &dir) || !dir.len)
    if (!stralloc_copys(&dir, ".") || !stralloc_0(&dir)) goto out;
  slash = !!strchr(pat.s, '/');
  dl = str_len(dir.s);

  roots[0] = dir.s;
  roots[1] = 0;
  fts = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, 0);
  if (!fts) {
    stralloc_copys(out, "unable to search ");
    stralloc_cats(out, dir.s);
    stralloc_cats(out, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  stralloc_copys(out, "");
  while ((e = fts_read(fts))) {
    if (e->fts_info == FTS_D && e->fts_level > 0 &&
        !str_diff(e->fts_name, ".git")) {
      fts_set(fts, e, FTS_SKIP);
      continue;
    }
    if (e->fts_info != FTS_F && e->fts_info != FTS_SL) continue;
    rel = e->fts_path;
    if (slash) {
      if (str_start(rel, dir.s) && rel[dl] == '/') rel += dl + 1;
      if (fnmatch(pat.s, rel, 0)) continue;
    } else if (fnmatch(pat.s, e->fts_name, 0))
      continue;
    if (hits == MAXHITS) { ++more; continue; }
    ++hits;
    stralloc_cats(out, e->fts_path);
    stralloc_append(out, '\n');
  }
  fts_close(fts);
  if (!hits) stralloc_cats(out, "no files matched");
  if (more) {
    stralloc_cats(out, "[");
    stralloc_catulong(out, more);
    stralloc_cats(out, " more not shown; narrow the pattern]");
  }
  ok = 1;
out:
  stralloc_free(&pat);
  stralloc_free(&dir);
  return ok;
}
