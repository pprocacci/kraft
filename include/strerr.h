#ifndef STRERR_H
#define STRERR_H

#include <sys/cdefs.h>

void strerr_die(int, const char *, const char *, const char *) __dead2;
void strerr_sys(int, const char *, const char *, const char *) __dead2;

#endif
