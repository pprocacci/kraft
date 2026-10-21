#ifndef MCP_H
#define MCP_H

#include <sys/types.h>
#include <signal.h>
#include <stddef.h>
#include "stralloc.h"

/*
 * A Model Context Protocol server on the stdio transport: a command
 * whose stdin and stdout carry JSON-RPC messages, one per line.
 */
struct mcp {
  const char *name;
  const char *cmd;
  pid_t pid;        /* 0 when not running */
  int to, from;     /* its stdin, its stdout */
  unsigned long id; /* the last request's */
  stralloc buf;     /* read but not yet used */
  stralloc line;
};

int mcp_start(struct mcp *, const char *, stralloc *);
int mcp_send(struct mcp *, const char *, size_t);
int mcp_rpc(struct mcp *, const char *, const char *, stralloc *,
    unsigned long, volatile sig_atomic_t *);
void mcp_stop(struct mcp *);

#endif
