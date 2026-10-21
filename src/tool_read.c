#include <errno.h>
#include <string.h>
#include "byte.h"
#include "file.h"
#include "fmt.h"
#include "stralloc.h"
#include "tool.h"

#define MAXLINE 2000

static int
fail(stralloc *out, const char *a, const char *b, const char *c)
{
  stralloc_copys(out, a);
  if (b) stralloc_cats(out, b);
  if (c) stralloc_cats(out, c);
  return 0;
}

/* read_file: path, optional offset (from 1) and limit */
int
tool_read(const char *in, size_t n, stralloc *out)
{
  stralloc path = STRALLOC_ZERO, f = STRALLOC_ZERO;
  unsigned long off = 1, lim = 2000, line = 0, shown = 0;
  char num[FMT_ULONG];
  size_t i, e, len, k;
  int ok = 0;

  if (!tool_arg(in, n, "path", &path)) return fail(out, "path is required", 0, 0);
  tool_argnum(in, n, "offset", &off);
  tool_argnum(in, n, "limit", &lim);
  if (!off) off = 1;
  if (!lim) lim = 2000;

  if (!file_get(path.s, &f)) {
    fail(out, "unable to read ", path.s, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  if (byte_chr(f.s, f.len < 8192 ? f.len : 8192, '\0') <
      (f.len < 8192 ? f.len : 8192)) {
    fail(out, path.s, " is a binary file", 0);
    goto out;
  }
  if (!stralloc_copys(out, "")) goto out;
  for (i = 0; i < f.len; i = e + 1) {
    e = i + byte_chr(f.s + i, f.len - i, '\n');
    if (++line < off) continue;
    if (shown == lim) continue;  /* keep counting lines */
    ++shown;
    k = fmt_ulong(num, line);
    for (len = k; len < 6; ++len) stralloc_append(out, ' ');
    stralloc_catb(out, num, k);
    stralloc_append(out, '\t');
    len = e - i;
    if (len > MAXLINE) {
      stralloc_catb(out, f.s + i, MAXLINE);
      stralloc_cats(out, " [line truncated]");
    } else
      stralloc_catb(out, f.s + i, len);
    stralloc_append(out, '\n');
  }
  if (!f.len) stralloc_cats(out, "(the file is empty)\n");
  else if (!shown) {
    stralloc_cats(out, "(the file has only ");
    stralloc_catulong(out, line);
    stralloc_cats(out, " lines)\n");
  } else if (off + shown - 1 < line) {
    stralloc_cats(out, "[showing lines ");
    stralloc_catulong(out, off);
    stralloc_cats(out, "-");
    stralloc_catulong(out, off + shown - 1);
    stralloc_cats(out, " of ");
    stralloc_catulong(out, line);
    stralloc_cats(out, "; use offset to read more]\n");
  }
  ok = 1;
out:
  stralloc_free(&path);
  stralloc_free(&f);
  return ok;
}
