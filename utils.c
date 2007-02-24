#include "sftpserver.h"

/* Error-checking workalike for fread().  Returns 0 on success, non-0 at
 * EOF. */
int do_fread(void *buffer, size_t size, size_t count, FILE *fp) {
  const size_t n = fread(buffer, size, count, fp);
  if(ferror(fp))
    fprintf(stderr, "read error: %s\n", strerror(errno));
  return n == count;
}

void *xmalloc(size_t n) {
  void *ptr;

  if(n) {
    if(!(ptr = malloc(n))) {
      fprintf(stderr, "out of memory\n");
      exit(-1);
    }
    return ptr;
  } else
    return 0;
}

void *xcalloc(size_t n, size_t size) {
  void *ptr;

  if(n && size) {
    if(!(ptr = calloc(n, size))) {
      fprintf(stderr, "out of memory\n");
      exit(-1);
    }
    return ptr;
  } else
    return 0;
}

void *xrecalloc(void *ptr, size_t n, size_t size) {
  if(n > SIZE_MAX / size) {
    fprintf(stderr, "out of memory\n");
    exit(-1);
  }
  n *= size;
  if(n) {
    if(!(ptr = realloc(ptr, n))) {
      fprintf(stderr, "out of memory\n");
      exit(-1);
    }
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

void *xrealloc(void *ptr, size_t n) {
  if(n) {
    if(!(ptr = realloc(ptr, n))) {
      fprintf(stderr, "out of memory\n");
      exit(-1);
    }
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

char *xstrdup(const char *s) {
  return strcpy(xmalloc(strlen(s) + 1), s);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
