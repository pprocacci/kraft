#include "alloc.h"
#include "stralloc.h"

void
stralloc_free(stralloc *sa)
{
  alloc_free(sa->s);
  sa->s = 0;
  sa->len = 0;
  sa->a = 0;
}
