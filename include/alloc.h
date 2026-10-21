#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

void *alloc(size_t);
void alloc_free(void *);
int alloc_re(void **, size_t);

#endif
