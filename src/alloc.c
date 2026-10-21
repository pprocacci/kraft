#include <errno.h>
#include <stdlib.h>
#include "alloc.h"

void *
alloc(size_t n)
{
  void *x;

  x = malloc(n ? n : 1);
  if (!x) errno = ENOMEM;
  return x;
}
