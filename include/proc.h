#ifndef PROC_H
#define PROC_H

#include <stddef.h>
#include "stralloc.h"

int proc_run(char *const *, unsigned long, size_t, stralloc *, int *);

#endif
