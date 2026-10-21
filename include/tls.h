#ifndef TLS_H
#define TLS_H

#include "stralloc.h"

int tls_connect(int, const char *, stralloc *);

#endif
