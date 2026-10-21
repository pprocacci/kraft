#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "buffer.h"
#include "str.h"
#include "stralloc.h"
#include "tls.h"

#define HANDSHAKE 30  /* seconds */

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

/* in the helper: tells the parent why, and gives up */
static void
fail(int st, const char *what)
{
  char e[256];

  ERR_error_string_n(ERR_get_error(), e, sizeof e);
  allwrite(st, what, str_len(what));
  allwrite(st, e, str_len(e));
  _exit(111);
}

/*
 * after the handshake the helper needs nothing but its two sockets,
 * so it gives up everything else: stdio goes to /dev/null, the
 * sockets may only be read, written and polled, and it enters
 * Capsicum capability mode, where it can open nothing new.  a kernel
 * without Capsicum just leaves it as it was.
 */
static void
sandbox(int net, int s)
{
  cap_rights_t rights;
  int fd;

  fd = open("/dev/null", O_RDWR);
  if (fd != -1) {
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2) close(fd);
  }
  cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_EVENT);
  cap_rights_limit(net, &rights);
  cap_rights_limit(s, &rights);
  cap_rights_init(&rights);
  cap_rights_limit(0, &rights);
  cap_rights_limit(1, &rights);
  cap_rights_limit(2, &rights);
  cap_enter();
}

/*
 * runs in the helper: the handshake, whose outcome goes to the parent
 * on st ("+" or the reason), then relaying bytes until either side
 * ends
 */
static void
helper(int net, int s, int st, const char *host)
{
  SSL_CTX *ctx;
  SSL *ssl;
  struct pollfd p[2];
  char buf[16384];
  ssize_t r;
  int n;

  /* the parent decides when to stop, by closing its end */
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGALRM, SIG_DFL);
  alarm(HANDSHAKE);
  if (!(ctx = SSL_CTX_new(TLS_client_method())))
    fail(st, "TLS setup failed: ");
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  if (!SSL_CTX_set_default_verify_paths(ctx))
    fail(st, "unable to load CA certificates: ");
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
  if (!(ssl = SSL_new(ctx))) fail(st, "TLS setup failed: ");
  SSL_set_fd(ssl, net);
  SSL_set_tlsext_host_name(ssl, host);
  if (!SSL_set1_host(ssl, host)) fail(st, "TLS setup failed: ");
  if (SSL_connect(ssl) != 1) fail(st, "TLS handshake failed: ");
  alarm(0);
  allwrite(st, "+", 1);
  close(st);
  sandbox(net, s);

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
 * net is closed in the caller.  returns the new socket once the
 * handshake is done, or -1 with errno set; if the handshake failed,
 * why holds the reason (and errno is ECONNABORTED).
 */
int
tls_connect(int net, const char *host, stralloc *why)
{
  char buf[512];
  int pair[2], st[2], status;
  ssize_t r;
  pid_t pid;

  if (!stralloc_copys(why, "")) return -1;
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) == -1)
    return -1;
  if (pipe2(st, O_CLOEXEC) == -1) {
    close(pair[0]);
    close(pair[1]);
    return -1;
  }
  pid = fork();
  if (pid == -1) {
    close(pair[0]);
    close(pair[1]);
    close(st[0]);
    close(st[1]);
    return -1;
  }
  if (pid == 0) {
    /* fork again so the helper is not our child to reap */
    close(pair[0]);
    close(st[0]);
    if (fork() != 0) _exit(0);
    helper(net, pair[1], st[1], host);
  }
  close(pair[1]);
  close(st[1]);
  close(net);
  while (waitpid(pid, &status, 0) == -1 && errno == EINTR) ;

  /* the handshake's outcome; ^C interrupts the wait */
  while ((r = read(st[0], buf, sizeof buf)) > 0)
    if (!stralloc_catb(why, buf, (size_t)r)) break;
  close(st[0]);
  if (why->len == 1 && why->s[0] == '+') {
    why->len = 0;
    return pair[0];
  }
  close(pair[0]);
  if (r == -1) return -1;  /* EINTR */
  if (!why->len) stralloc_copys(why, "TLS handshake timed out");
  stralloc_0(why);
  errno = ECONNABORTED;
  return -1;
}
