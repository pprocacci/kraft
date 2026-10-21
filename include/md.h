#ifndef MD_H
#define MD_H

#include <stddef.h>
#include "stralloc.h"

/* the state of Markdown being rendered for a terminal as it streams in */
struct md {
  int bol;        /* at the beginning of a line */
  int ticks;      /* backquotes seen at the beginning of a line */
  int hashes;     /* #s seen at the beginning of a line */
  int star;       /* one * seen, maybe half of ** */
  int bold;       /* inside **...** */
  int heading;    /* on a # heading line */
  int code;       /* inside `...` */
  int fence;      /* inside a ``` block */
  int fenceline;  /* on a ``` line itself */
};

#define MD_ZERO { 1, 0, 0, 0, 0, 0, 0, 0, 0 }

int md_put(struct md *, stralloc *, const char *, size_t);
int md_end(struct md *, stralloc *);

#endif
