#ifndef DIFF_H
#define DIFF_H

#include <stddef.h>
#include "stralloc.h"

int diff_lines(stralloc *, const char *, size_t, const char *, size_t,
    unsigned long);

#endif
