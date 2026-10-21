#ifndef ANTH_H
#define ANTH_H

#include <stddef.h>
#include "stralloc.h"

/* Anthropic Messages API: POST /v1/messages */

int anth_body(stralloc *, const char *, unsigned long, const char *, size_t);
int anth_delta(const char *, size_t, stralloc *, stralloc *);

#endif
