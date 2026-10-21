#ifndef ANTH_H
#define ANTH_H

#include <stddef.h>
#include "api.h"

/* Anthropic Messages API: POST /v1/messages */

int anth_render(stralloc *, const struct req *);
int anth_event(struct msg *, const char *, size_t, struct reply *);

#endif
