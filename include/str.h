#ifndef STR_H
#define STR_H

#include <stddef.h>

size_t str_len(const char *);
int str_diff(const char *, const char *);
int str_start(const char *, const char *);
int str_casestart(const char *, const char *);

#endif
