#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include "tcp.h"

/* returns a connected socket, or -1 with errno set */
int
tcp_connect(const char *host, const char *port)
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
    fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC,
        ai->ai_protocol);
    if (fd == -1) { e = errno; continue; }
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    e = errno;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd == -1) errno = e;
  return fd;
}
