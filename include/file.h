#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include "stralloc.h"

int file_get(const char *, stralloc *);
int file_put(const char *, const char *, size_t);

#endif
