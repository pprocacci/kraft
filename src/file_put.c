#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>  /* rename */
#include <stdlib.h>
#include <unistd.h>
#include "buffer.h"
#include "file.h"
#include "stralloc.h"

/*
 * replaces the contents of path with s, atomically: the data goes to
 * a temporary file that is then renamed over the target.  symbolic
 * links are followed, and an existing file keeps its mode.  returns
 * 1 on success, 0 with errno set.
 */
int
file_put(const char *path, const char *s, size_t n)
{
  char real[PATH_MAX];
  stralloc tmp = STRALLOC_ZERO;
  struct stat st;
  int fd = -1, exists, e;
  ssize_t w;

  if (realpath(path, real)) path = real;
  exists = stat(path, &st) == 0;
  if (!stralloc_copys(&tmp, path)) return 0;
  if (!stralloc_cats(&tmp, ".kraft-tmp")) goto bad;
  if (!stralloc_0(&tmp)) goto bad;

  fd = open(tmp.s, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  if (fd == -1) goto bad;
  while (n) {
    w = buffer_unixwrite(fd, (char *)s, n);
    if (w <= 0) { if (!w) errno = EIO; goto bad; }
    s += w;
    n -= (size_t)w;
  }
  if (exists && fchmod(fd, st.st_mode & 07777) == -1) goto bad;
  if (fsync(fd) == -1) goto bad;
  if (close(fd) == -1) { fd = -1; goto bad; }
  fd = -1;
  if (rename(tmp.s, path) == -1) goto bad;
  stralloc_free(&tmp);
  return 1;
bad:
  e = errno;
  if (fd != -1) close(fd);
  if (tmp.s) unlink(tmp.s);
  stralloc_free(&tmp);
  errno = e;
  return 0;
}
