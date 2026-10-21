#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include "tcp.h"

/* connects fd to ai within secs seconds; 0 with errno set if not */
static int
attempt(int fd, const struct addrinfo *ai, unsigned secs)
{
  struct pollfd p;
  socklen_t len;
  int e, r;

  if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) return 1;
  if (errno != EINPROGRESS) return 0;
  p.fd = fd;
  p.events = POLLOUT;
  r = poll(&p, 1, (int)secs * 1000);
  if (r == -1) return 0;  /* EINTR: the caller was interrupted */
  if (r == 0) { errno = ETIMEDOUT; return 0; }
  len = sizeof e;
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &e, &len) == -1) return 0;
  if (e) { errno = e; return 0; }
  return 1;
}

/*
 * returns a socket connected to host and port, trying each address
 * for at most secs seconds, or -1 with errno set.  the socket is
 * blocking and close-on-exec.
 */
int
tcp_connect(const char *host, const char *port, unsigned secs)
{
  struct addrinfo hints = { 0 }, *res, *ai;
  int fd = -1, e = ECONNREFUSED;

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &hints, &res) != 0) {
    errno = EHOSTUNREACH;
    return -1;
  }
  for (ai = res; ai; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC |
        SOCK_NONBLOCK, ai->ai_protocol);
    if (fd == -1) { e = errno; continue; }
    if (attempt(fd, ai, secs) &&
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK) != -1)
      break;
    e = errno;
    close(fd);
    fd = -1;
    if (e == EINTR) break;
  }
  freeaddrinfo(res);
  if (fd == -1) errno = e;
  return fd;
}
