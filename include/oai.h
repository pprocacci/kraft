#ifndef OAI_H
#define OAI_H

#include <stddef.h>
#include "stralloc.h"

/* OpenAI-compatible Chat Completions API: POST /v1/chat/completions */

int oai_body(stralloc *, const char *, unsigned long, const char *, size_t);
int oai_delta(const char *, size_t, stralloc *, stralloc *);

#endif
