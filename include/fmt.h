#ifndef FMT_H
#define FMT_H

#include <stddef.h>

#define FMT_ULONG 40 /* enough for any unsigned long, plus slack */

size_t fmt_ulong(char *, unsigned long);

#endif
