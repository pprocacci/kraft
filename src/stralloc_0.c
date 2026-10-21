#include "stralloc.h"

/* appends a NUL; note that this counts toward sa->len */
int
stralloc_0(stralloc *sa)
{
  return stralloc_append(sa, '\0');
}
