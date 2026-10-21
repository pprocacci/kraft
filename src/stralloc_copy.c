#include "stralloc.h"

int
stralloc_copy(stralloc *sa, const stralloc *from)
{
  return stralloc_copyb(sa, from->s, from->len);
}
