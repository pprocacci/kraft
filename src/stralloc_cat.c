#include "stralloc.h"

int
stralloc_cat(stralloc *sa, const stralloc *t)
{
  return stralloc_catb(sa, t->s, t->len);
}
