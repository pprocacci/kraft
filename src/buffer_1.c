#include "buffer.h"

static char x[BUFFER_OUTSIZE];

buffer buffer_1[1] = { BUFFER_INIT(buffer_unixwrite, 1, x, sizeof x) };
