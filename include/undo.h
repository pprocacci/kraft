#ifndef UNDO_H
#define UNDO_H

#include "stralloc.h"

/*
 * Undo snapshots: before a tool changes a file, its contents (or that
 * it did not exist) are kept in a directory of numbered entries, each
 * tagged with the task that made the change.
 */

int undo_save(const char *, unsigned long, unsigned long, const char *);
int undo_load(const char *, unsigned long, unsigned long *, stralloc *,
    stralloc *, int *);

#endif
