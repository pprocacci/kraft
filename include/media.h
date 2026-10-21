#ifndef MEDIA_H
#define MEDIA_H

#include <stddef.h>
#include "stralloc.h"

const char *media_type(const char *, size_t);
int base64_enc(stralloc *, const char *, size_t);

#endif
