#include "fmt.h"
#include "stralloc.h"

int
stralloc_catulong(stralloc *sa, unsigned long u)
{
  char buf[FMT_ULONG];

  return stralloc_catb(sa, buf, fmt_ulong(buf, u));
}
