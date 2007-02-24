#include "sftpserver.h"
#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void *xmalloc(size_t n) {
  void *ptr;

  if(n) {
    if(!(ptr = malloc(n)))
      fatal("out of memory");
    return ptr;
  } else
    return 0;
}

void *xcalloc(size_t n, size_t size) {
  void *ptr;

  if(n && size) {
    if(!(ptr = calloc(n, size)))
      fatal("out of memory");
    return ptr;
  } else
    return 0;
}

void *xrecalloc(void *ptr, size_t n, size_t size) {
  if(n > SIZE_MAX / size)
      fatal("out of memory");
  n *= size;
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      fatal("out of memory");
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

void *xrealloc(void *ptr, size_t n) {
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      fatal("out of memory");
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

char *xstrdup(const char *s) {
  return strcpy(xmalloc(strlen(s) + 1), s);
}

void fatal(const char *msg, ...) {
  va_list ap;

  fprintf(stderr, "FATAL: ");
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(-1);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
