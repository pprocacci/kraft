#ifndef URL_H
#define URL_H

#include "stralloc.h"

struct url {
  int tls;        /* 1 for https */
  stralloc host;  /* without brackets, NUL-terminated */
  stralloc port;  /* NUL-terminated */
  stralloc path;  /* base path, no trailing slash, may be empty */
};

int url_parse(struct url *, const char *);

#endif
