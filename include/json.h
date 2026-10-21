#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include "stralloc.h"

int json_str(stralloc *, const char *, size_t);
size_t json_skip(const char *, size_t, size_t);
int json_get(const char *, size_t, const char *, const char **, size_t *);
int json_unstr(stralloc *, const char *, size_t);

#endif
