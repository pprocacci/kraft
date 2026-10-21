#ifndef CFG_H
#define CFG_H

#include <stddef.h>
#include "stralloc.h"

/*
 * Config files: one setting per line, a key and the rest of the line
 * as its value ("model claude-opus-5", "allow bash make test*").
 * Blank lines and lines starting with # are skipped.
 */

unsigned long cfg_parse(const char *, size_t,
    int (*)(void *, const char *, const char *, unsigned long), void *);
int sha256_hex(stralloc *, const char *, size_t);

#endif
