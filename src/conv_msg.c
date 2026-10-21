#include <errno.h>
#include <stdint.h>
#include "alloc.h"
#include "conv.h"

/*
 * starts a message with the given role.  consecutive user messages
 * are merged: the last message is returned if it is already a user
 * message.  returns 0 if out of memory.
 */
struct msg *
conv_msg(struct conv *c, int role)
{
  static const struct msg zero;
  struct msg *m;
  void *p;
  size_t a;

  if (role == ROLE_USER && c->n && c->m[c->n - 1].role == ROLE_USER)
    return &c->m[c->n - 1];
  if (c->n == c->a) {
    a = c->a ? c->a * 2 : 16;
    if (a > SIZE_MAX / sizeof *m) { errno = ENOMEM; return 0; }
    p = c->m;
    if (!alloc_re(&p, a * sizeof *m)) return 0;
    c->m = p;
    c->a = a;
  }
  m = &c->m[c->n++];
  *m = zero;
  m->role = role;
  return m;
}
