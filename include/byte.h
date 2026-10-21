#ifndef BYTE_H
#define BYTE_H

#include <stddef.h>

void byte_copy(void *, size_t, const void *);
size_t byte_chr(const char *, size_t, int);
int byte_diff(const void *, size_t, const void *);

#endif
