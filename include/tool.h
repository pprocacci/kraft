#ifndef TOOL_H
#define TOOL_H

#include <stddef.h>
#include "stralloc.h"

/*
 * A tool the model may call.  run gets the input as a JSON object
 * and puts its output in out; it returns 1 on success, 0 on failure
 * (out then holds the reason).
 */
struct tool {
  const char *name;
  const char *desc;
  const char *schema;  /* JSON Schema of the input */
  int ask;             /* needs the user's approval */
  int (*run)(const char *, size_t, stralloc *);
};

extern const struct tool tool_all[];  /* ends with a null name */

int tool_arg(const char *, size_t, const char *, stralloc *);
int tool_argnum(const char *, size_t, const char *, unsigned long *);
int tool_argflag(const char *, size_t, const char *);

int tool_read(const char *, size_t, stralloc *);
int tool_write(const char *, size_t, stralloc *);
int tool_edit(const char *, size_t, stralloc *);
int tool_bash(const char *, size_t, stralloc *);
int tool_find(const char *, size_t, stralloc *);
int tool_grep(const char *, size_t, stralloc *);
int tool_ls(const char *, size_t, stralloc *);

#endif
