#ifndef CONV_H
#define CONV_H

#include <stddef.h>
#include "stralloc.h"

/*
 * A conversation in a form neutral to the API.  Each backend renders
 * it into its own request format and fills in assistant messages from
 * its own stream format.
 */

#define ROLE_USER 0
#define ROLE_ASSISTANT 1

#define BLK_TEXT 0    /* text */
#define BLK_THINK 1   /* text: reasoning; aux: signature (Anthropic) */
#define BLK_TOOL 2    /* id, name; text: input as JSON */
#define BLK_RESULT 3  /* id; text: tool output; iserr */
#define BLK_RAW 4     /* text: a raw Anthropic content block, sent back as is */

struct block {
  int type;
  int iserr;
  long idx;       /* position in the API's stream, or -1 */
  stralloc id;
  stralloc name;
  stralloc text;
  stralloc aux;
};

struct msg {
  int role;
  struct block *b;
  size_t n;
  size_t a;
};

struct conv {
  struct msg *m;
  size_t n;
  size_t a;
};

struct msg *conv_msg(struct conv *, int);
struct block *conv_block(struct msg *, int);
void conv_drop(struct conv *);
int conv_save(stralloc *, const struct conv *);
int conv_load(struct conv *, const char *, size_t);

#endif
