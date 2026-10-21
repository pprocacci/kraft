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
  const char *raw;     /* a server tool's whole definition, or 0 */
  int mcp;             /* an MCP server's tool: the server, from 1 */
  const char *mname;   /* and its name there */
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
int tool_fetch(const char *, size_t, stralloc *);
int tool_multiedit(const char *, size_t, stralloc *);
int tool_todo(const char *, size_t, stralloc *);
int tool_jobout(const char *, size_t, stralloc *);
int tool_jobkill(const char *, size_t, stralloc *);
int tool_ask(const char *, size_t, stralloc *);
int tool_symbols(const char *, size_t, stralloc *);

/* Universal Ctags, for tool_symbols (devel/universal-ctags) */
#define UCTAGS "/usr/local/bin/uctags"
extern const struct tool tool_symbol;  /* offered when UCTAGS is there */

#endif
