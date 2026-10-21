#ifndef SCAN_H
#define SCAN_H

#include <stddef.h>

size_t scan_ulong(const char *, unsigned long *);
size_t scan_ulongb(const char *, size_t, unsigned long *);
size_t scan_xlong(const char *, unsigned long *);

#endif
