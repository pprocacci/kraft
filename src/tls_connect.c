#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "buffer.h"
#include "strerr.h"
#include "tls.h"

#define FATAL "kraft: fatal: "

static int
allwrite(int fd, const char *s, size_t n)
{
  ssize_t w;

  while (n) {
    w = buffer_unixwrite(fd, (char *)s, n);
    if (w <= 0) return 0;
    s += w;
    n -= (size_t)w;
  }
  return 1;
}

static void
fail(const char *what)
{
  char e[256];

  ERR_error_string_n(ERR_get_error(), e, sizeof e);
  strerr_die(111, FATAL, what, e);
}

/* runs in the helper: handshake, then pump bytes until either side ends */
static void
helper(int net, int s, const char *host)
{
  SSL_CTX *ctx;
  SSL *ssl;
  struct pollfd p[2];
  char buf[16384], e[256];
  ssize_t r;
  int n;

  if (!(ctx = SSL_CTX_new(TLS_client_method()))) fail("TLS setup failed: ");
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  if (!SSL_CTX_set_default_verify_paths(ctx))
    fail("unable to load CA certificates: ");
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
  if (!(ssl = SSL_new(ctx))) fail("TLS setup failed: ");
  SSL_set_fd(ssl, net);
  SSL_set_tlsext_host_name(ssl, host);
  if (!SSL_set1_host(ssl, host)) fail("TLS setup failed: ");
  if (SSL_connect(ssl) != 1) {
    ERR_error_string_n(ERR_get_error(), e, sizeof e);
    buffer_puts(buffer_2, FATAL "TLS handshake with ");
    buffer_puts(buffer_2, host);
    strerr_die(111, " failed: ", e, 0);
  }

  p[0].fd = net;
  p[0].events = POLLIN;
  p[1].fd = s;
  p[1].events = POLLIN;
  for (;;) {
    if (SSL_pending(ssl)) {
      p[0].revents = POLLIN;
      p[1].revents = 0;
    } else if (poll(p, 2, -1) == -1) {
      if (errno == EINTR) continue;
      break;
    }
    if (p[0].revents) {
      n = SSL_read(ssl, buf, sizeof buf);
      if (n <= 0) {
        n = SSL_get_error(ssl, n);
        if (n == SSL_ERROR_WANT_READ || n == SSL_ERROR_WANT_WRITE) continue;
        break;
      }
      if (!allwrite(s, buf, (size_t)n)) break;
    }
    if (p[1].revents) {
      r = buffer_unixread(s, buf, sizeof buf);
      if (r <= 0) break;
      if (SSL_write(ssl, buf, (int)r) <= 0) break;
    }
  }
  _exit(0);
}

/*
 * wraps the connected socket net in TLS, verifying that the server's
 * certificate is valid for host.  the TLS session lives in a helper
 * process; the caller gets a plain socket to read and write instead.
 * net is closed in the caller.  returns the new socket, or -1.
 */
int
tls_connect(int net, const char *host)
{
  int pair[2];
  pid_t pid;
  int status;

  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) == -1)
    return -1;
  pid = fork();
  if (pid == -1) {
    close(pair[0]);
    close(pair[1]);
    return -1;
  }
  if (pid == 0) {
    /* fork again so the helper is not our child to reap */
    close(pair[0]);
    if (fork() != 0) _exit(0);
    helper(net, pair[1], host);
  }
  close(pair[1]);
  close(net);
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR) ;
  return pair[0];
}
