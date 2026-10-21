#ifndef MCP_H
#define MCP_H

#include <sys/types.h>
#include <signal.h>
#include <stddef.h>
#include "stralloc.h"
#include "url.h"

/*
 * A Model Context Protocol server: a command whose stdin and stdout
 * carry JSON-RPC messages, one per line; or, if cmd is an http or
 * https URL, a server on the streamable HTTP transport.
 */
struct mcp {
  const char *name;
  const char *cmd;
  stralloc hdrs;    /* extra request headers, each ending in CRLF */
  int up;           /* running, or reachable */
  pid_t pid;        /* the command's */
  int to, from;     /* its stdin, its stdout */
  int http;         /* cmd is a URL */
  struct url u;
  stralloc hosthdr; /* host[:port], for Host: */
  stralloc session; /* Mcp-Session-Id, or empty */
  stralloc token;   /* an OAuth access token, or empty */
  int status;       /* the last HTTP status */
  stralloc auth;    /* and its WWW-Authenticate, NUL-terminated */
  unsigned long id; /* the last request's */
  stralloc buf;     /* read but not yet used */
  stralloc line;
};

int mcp_start(struct mcp *, const char *, stralloc *);
int mcp_send(struct mcp *, const char *, size_t);
int mcp_rpc(struct mcp *, const char *, const char *, stralloc *,
    unsigned long, volatile sig_atomic_t *);
int mcp_take(struct mcp *, const char *, size_t, const char *, size_t,
    stralloc *);
int mcp_post(struct mcp *, const char *, size_t, const char *, size_t,
    stralloc *, volatile sig_atomic_t *);
void mcp_stop(struct mcp *);

#endif
