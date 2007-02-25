#ifndef ALLOC_H
#define ALLOC_H

struct chunk;

struct allocator {
  struct chunk *chunks;
};

struct allocator *alloc_init(struct allocator *a);
/* Initialize allocator A */

void *alloc(struct allocator *a, size_t n);
/* Allocate N bytes from A.  The new space is 0-filled. */

void alloc_destroy(struct allocator *a);
/* Free all memory used by A and re-initialize */

#endif /* ALLOC_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
