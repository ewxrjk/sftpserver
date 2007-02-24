#include "sftpserver.h"
#include "alloc.h"
#include "utils.h"
#include <stdlib.h>

union block;

struct chunk {
  struct chunk *next;
  union block *ptr;
  size_t left;
};

union block {
  int i;
  long l;
  float f;
  double d;
  void *vp;
  double *ip;
  int (*fnp)(void);
  struct chunk c;
};

#define NBLOCKS 512

void alloc_init(struct allocator *a) {
  a->chunks = 0;
}

void *alloc(struct allocator *a, size_t n) {
  /* calculate number of blocks */
  const size_t m = n / sizeof (union block) + !!(n % sizeof (union block));
  struct chunk *c;

  if(!m) return 0;                      /* overflowed */
  /* See if there's enough room */
  if(!(c = a->chunks) || c->left < m) {
    /* Make sure we allocate enough space */
    const size_t cs = m > NBLOCKS ? m : NBLOCKS;
    /* xcalloc -> calloc which 0-fills */
    union block *const nb = xcalloc(cs, sizeof (union block));

    c = &nb->c;
    c->next = a->chunks;
    c->ptr = nb + 1;
    c->left = cs;
    a->chunks = c;
  }
  c->left -= m;
  c->ptr += m;
  return c->ptr - m;
}

void alloc_destroy(struct allocator *a) {
  struct chunk *c, *d;

  c = a->chunks;
  while((d = c)) {
    c = c->next;
    free(d);
  }
  a->chunks = 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
