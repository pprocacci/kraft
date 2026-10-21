#ifndef PROC_H
#define PROC_H

#include <stddef.h>
#include <sys/types.h>
#include "stralloc.h"

int proc_run(char *const *, unsigned long, size_t, stralloc *, int *);
void proc_limit(void);

/* the process group proc_run is waiting on, or 0; for signal handlers */
extern volatile pid_t proc_pid;

#endif
