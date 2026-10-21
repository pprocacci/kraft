#ifndef OAI_H
#define OAI_H

#include <stddef.h>
#include "api.h"

/* OpenAI-compatible Chat Completions API: POST /v1/chat/completions */

int oai_render(stralloc *, const struct req *);
int oai_event(struct msg *, const char *, size_t, struct reply *);

#endif
