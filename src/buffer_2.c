#include "buffer.h"

static char x[256];

buffer buffer_2[1] = { BUFFER_INIT(buffer_unixwrite, 2, x, sizeof x) };
