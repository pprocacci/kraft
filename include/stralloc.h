#ifndef STRALLOC_H
#define STRALLOC_H

#include <stddef.h>

typedef struct stralloc {
  char *s;
  size_t len;
  size_t a;
} stralloc;

#define STRALLOC_ZERO { 0, 0, 0 }

int stralloc_ready(stralloc *, size_t);
int stralloc_readyplus(stralloc *, size_t);
int stralloc_copyb(stralloc *, const char *, size_t);
int stralloc_copys(stralloc *, const char *);
int stralloc_copy(stralloc *, const stralloc *);
int stralloc_catb(stralloc *, const char *, size_t);
int stralloc_cats(stralloc *, const char *);
int stralloc_cat(stralloc *, const stralloc *);
int stralloc_append(stralloc *, char);
int stralloc_0(stralloc *);
int stralloc_catulong(stralloc *, unsigned long);
void stralloc_free(stralloc *);

#endif
