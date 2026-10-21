#ifndef JOB_H
#define JOB_H

#include <stddef.h>
#include <sys/types.h>
#include "stralloc.h"

/*
 * Background commands.  Each runs with /bin/sh in its own process
 * group, its output going to a private temporary file that job_output
 * reads from where it last left off.
 */

#define JOB_MAX 16

struct job {
  pid_t pid;       /* 0: the slot is free */
  int done;        /* it has exited, and been reaped */
  int status;      /* exit status, or 128 plus the signal */
  off_t off;       /* output read so far */
  stralloc path;   /* the output file, NUL-terminated */
  stralloc cmd;    /* the command, NUL-terminated */
};

extern struct job job_tab[JOB_MAX];  /* job n is job_tab[n - 1] */

int job_start(const char *, stralloc *);
int job_output(int, unsigned long, size_t, stralloc *);
int job_kill(int);
void job_killall(void);

#endif
