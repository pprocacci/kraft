#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <sys/types.h>

typedef struct buffer {
  char *x;   /* storage */
  size_t a;  /* size of x */
  size_t p;  /* output: bytes pending; input: next unread byte */
  size_t n;  /* input: bytes held in x */
  int fd;
  ssize_t (*op)(int, char *, size_t);
} buffer;

#define BUFFER_INIT(op, fd, x, a) { (x), (a), 0, 0, (fd), (op) }
#define BUFFER_INSIZE 8192
#define BUFFER_OUTSIZE 8192

void buffer_init(buffer *, ssize_t (*)(int, char *, size_t), int, char *, size_t);
int buffer_put(buffer *, const char *, size_t);
int buffer_puts(buffer *, const char *);
int buffer_flush(buffer *);
ssize_t buffer_get(buffer *, char *, size_t);

ssize_t buffer_unixread(int, char *, size_t);
ssize_t buffer_unixwrite(int, char *, size_t);

extern buffer buffer_1[1];
extern buffer buffer_2[1];

#endif
