#include "sftpserver.h"
#include "utils.h"
#include "debug.h"
#include "alloc.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

int do_read(int fd, void *buffer, size_t size) {
  size_t sofar = 0;
  ssize_t n;
  char *ptr = buffer;

  while(sofar < size) {
    n = read(fd, ptr + sofar, size - sofar);
    if(n > 0)
      sofar += n;
    else if(n == 0)
      return -1;                        /* eof */
    else
      fatal("read error: %s", strerror(errno));
  }
  return 0;                             /* ok */
}

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

static void (*exitfn)(int) attribute((noreturn)) = exit;

void fatal(const char *msg, ...) {
  va_list ap;

  fprintf(stderr, "FATAL: ");
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exitfn(-1);
}

pid_t xfork(void) {
  pid_t pid;

  if((pid = fork()) < 0)
    fatal("fork: %s", strerror(errno));
  if(!pid)
    exitfn = _exit;
  return pid;
}

char *appendn(struct allocator *a, char *s, size_t *ns,
              const char *t, size_t lt) {
  const size_t ls = s ? strlen(s) : 0, need = lt + ls + 1;
  
  if(need > *ns) {
    size_t newsize = *ns ? *ns : 16;

    while(need > newsize && newsize)
      newsize *= 2;
    if(!newsize) fatal("out of memory");
    s = allocmore(a, s, *ns, newsize);
    *ns = newsize;
  }
  memcpy(s + ls, t, lt);
  s[ls + lt] = 0;
  return s;
}

char *append(struct allocator *a, char *s, size_t *ns, 
             const char *t) {
  return appendn(a, s, ns, t, strlen(t));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
