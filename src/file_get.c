#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "buffer.h"
#include "file.h"
#include "stralloc.h"

#define MAXFILE (16 * 1024 * 1024)

/* reads all of a regular file into out; 1 on success, 0 with errno */
int
file_get(const char *path, stralloc *out)
{
  struct stat st;
  ssize_t r;
  int fd, e;

  if (!stralloc_copys(out, "")) return 0;
  fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) return 0;
  if (fstat(fd, &st) == -1) goto bad;
  if (S_ISDIR(st.st_mode)) { errno = EISDIR; goto bad; }
  if (st.st_size > MAXFILE) { errno = EFBIG; goto bad; }
  for (;;) {
    if (!stralloc_readyplus(out, 8192)) goto bad;
    r = buffer_unixread(fd, out->s + out->len, 8192);
    if (r == -1) goto bad;
    if (r == 0) break;
    out->len += (size_t)r;
    if (out->len > MAXFILE) { errno = EFBIG; goto bad; }
  }
  close(fd);
  return 1;
bad:
  e = errno;
  close(fd);
  errno = e;
  return 0;
}
