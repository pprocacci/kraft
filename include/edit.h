#ifndef EDIT_H
#define EDIT_H

#include <stddef.h>
#include "stralloc.h"

unsigned long edit_apply(stralloc *, const stralloc *, const stralloc *, int,
    stralloc *);

#endif
