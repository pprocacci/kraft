#ifndef PERM_H
#define PERM_H

#include <stddef.h>
#include "stralloc.h"

/* what perm_auto() decides */
#define PERM_ALLOW 1   /* safe: allow without asking */
#define PERM_UNSURE 0  /* the rules cannot tell; a model may judge */
#define PERM_ASK -1    /* risky: only the user may allow it */

int perm_auto(const char *, const char *, size_t, stralloc *);

#endif
