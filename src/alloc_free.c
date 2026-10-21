#include <stdlib.h>
#include "alloc.h"

void
alloc_free(void *x)
{
  free(x);
}
