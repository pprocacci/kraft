#ifndef SSE_H
#define SSE_H

#include <stddef.h>
#include "http.h"
#include "stralloc.h"

struct sse {
  stralloc line;
  char x[4096];
  size_t p;
  size_t n;
  int eof;
};

#define SSE_ZERO { STRALLOC_ZERO, { 0 }, 0, 0, 0 }

int sse_get(struct sse *, struct http *, stralloc *, stralloc *);

#endif
