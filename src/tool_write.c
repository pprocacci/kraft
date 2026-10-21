#include <errno.h>
#include <string.h>
#include "file.h"
#include "path.h"
#include "stralloc.h"
#include "tool.h"

/* write_file: path, content */
int
tool_write(const char *in, size_t n, stralloc *out)
{
  stralloc path = STRALLOC_ZERO, body = STRALLOC_ZERO;
  int ok = 0;

  if (!tool_arg(in, n, "path", &path)) {
    stralloc_copys(out, "path is required");
    goto out;
  }
  if (!tool_arg(in, n, "content", &body)) {
    stralloc_copys(out, "content is required");
    goto out;
  }
  if (path_secret(path.s)) {
    stralloc_copys(out, "refused: ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, " is one of kraft's private files");
    goto out;
  }
  if (!file_put(path.s, body.s, body.len)) {
    stralloc_copys(out, "unable to write ");
    stralloc_cats(out, path.s);
    stralloc_cats(out, ": ");
    stralloc_cats(out, strerror(errno));
    goto out;
  }
  stralloc_copys(out, "wrote ");
  stralloc_catulong(out, (unsigned long)body.len);
  stralloc_cats(out, " bytes to ");
  stralloc_cats(out, path.s);
  ok = 1;
out:
  stralloc_free(&path);
  stralloc_free(&body);
  return ok;
}
