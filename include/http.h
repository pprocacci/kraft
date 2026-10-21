#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <sys/types.h>
#include "buffer.h"
#include "stralloc.h"

struct http {
  int fd;
  buffer b;                /* bytes from the server */
  char x[BUFFER_INSIZE];
  int status;              /* e.g. 200 */
  int chunked;             /* Transfer-Encoding: chunked */
  int haslen;              /* Content-Length seen */
  int started;             /* at least one chunk read */
  int eof;                 /* body fully read */
  unsigned long left;      /* bytes left in chunk or body */
  unsigned long retry;     /* Retry-After in seconds, or 0 */
  stralloc loc;            /* Location, NUL-terminated, or empty */
  stralloc ctype;          /* Content-Type, NUL-terminated, or empty */
  stralloc session;        /* Mcp-Session-Id, NUL-terminated, or empty */
};

int http_request(int, const char *, const char *, const char *,
    const char *, const char *, size_t);
int http_response(struct http *, int);
int http_getline(struct http *, stralloc *);
ssize_t http_get(struct http *, char *, size_t);

#endif
