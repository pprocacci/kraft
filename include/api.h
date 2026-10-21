#ifndef API_H
#define API_H

#include <stddef.h>
#include "conv.h"
#include "stralloc.h"
#include "tool.h"

/* what a backend's event function says about one stream event */
#define API_MORE 1   /* keep reading */
#define API_DONE 0   /* the reply is complete */
#define API_ERROR -1 /* failed; the message is in reply.err */

/* why the model stopped */
#define STOP_NONE 0
#define STOP_END 1      /* finished its turn */
#define STOP_TOOL 2     /* wants tools run */
#define STOP_MAX 3      /* hit max_tokens */
#define STOP_REFUSAL 4  /* declined */
#define STOP_OTHER 5

struct req {
  const char *model;
  unsigned long max;
  const char *system;
  const struct tool *tools;  /* ends with a null name */
  const struct conv *conv;
};

/* what one stream event produced, for display */
struct reply {
  stralloc text;   /* reply text */
  stralloc think;  /* reasoning */
  stralloc err;    /* error message */
  int stop;
};

struct api {
  const char *name;      /* as given to -a */
  const char *endpoint;  /* appended to the base URL path */
  const char *keyenv;    /* environment variable holding the key */
  const char *keyhdr;    /* header prefix for the key */
  const char *hdrs;      /* extra header lines, each ending in CRLF */
  int (*render)(stralloc *, const struct req *);
  int (*event)(struct msg *, const char *, size_t, struct reply *);
};

#endif
