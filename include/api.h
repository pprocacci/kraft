#ifndef API_H
#define API_H

#include <stddef.h>
#include "stralloc.h"

/* what a backend's delta function says about one stream event */
#define API_MORE 1   /* keep reading */
#define API_DONE 0   /* the reply is complete */
#define API_ERROR -1 /* failed; the message has been put in text */

struct api {
  const char *name;      /* as given to -a */
  const char *endpoint;  /* appended to the base URL path */
  const char *keyenv;    /* environment variable holding the key */
  const char *keyhdr;    /* header prefix for the key */
  const char *hdrs;      /* extra header lines, each ending in CRLF */
  int (*body)(stralloc *, const char *, unsigned long, const char *, size_t);
  int (*delta)(const char *, size_t, stralloc *, stralloc *);
};

#endif
