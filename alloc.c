/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "sftpserver.h"
#include "alloc.h"
#include "utils.h"
#include "debug.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

union block;

struct chunk {
  struct chunk *next;
  union block *ptr;
  size_t left;
  /* size_t will usually have the same size as a pointer; by chucking an extra
   * one in we become 4 * the size of a pointer, which is much more likely to
   * be a power of 2 than 3 *. */
  size_t spare;
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

struct allocator *sftp_alloc_init(struct allocator *a) {
  a->chunks = 0;
  return a;
}

/* Convert NBYTES to a block count */
static inline size_t blocks(size_t nbytes) {
  return nbytes / sizeof (union block) + !!(nbytes % sizeof (union block));
}

void *sftp_alloc(struct allocator *a, size_t n) {
  /* calculate number of blocks */
  const size_t m = blocks(n);
  struct chunk *c;

  if(!m)
    return 0;
  assert(a != 0);
  /* See if there's enough room */
  if(!(c = a->chunks) || c->left < m) {
    /* Make sure we allocate enough space */
    const size_t cs = m >= NBLOCKS ? m + 1 : NBLOCKS;
    /* xcalloc -> calloc which 0-fills */
    union block *nb;

    if(!cs)
      fatal("out of memory");
    nb = xcalloc(cs, sizeof (union block));
    c = &nb->c;
    c->next = a->chunks;
    c->ptr = nb + 1;
    c->left = cs - 1;
    a->chunks = c;
  }
  assert(m <= c->left);
  /* We always return 0-filled memory.  In this case we fill by block, which is
   * guaranteed to be at least enough (compare below). */
  memset(c->ptr, 0, m * sizeof (union block));
  c->left -= m;
  c->ptr += m;
  return c->ptr - m;
}

void *sftp_alloc_more(struct allocator *a, void *ptr, size_t oldn, size_t newn) {
  const size_t oldm = blocks(oldn), newm = blocks(newn);
  void *newptr;

  if(ptr) {
    assert(a->chunks != 0);
    D(("ptr=%p oldm=%zu a->chunks->ptr=%p blocksize=%zu",
       ptr, oldm, a->chunks->ptr, sizeof(union block)));
    if((union block *)ptr + oldm == a->chunks->ptr) {
      /* ptr is the most recently allocated block.  We could do better and
       * search all the chunks for it. */
      if(newm <= oldm) {
        /* We can always shrink.  We capture the no-change case here too. */
        a->chunks->ptr -= oldm - newm;
        a->chunks->left += oldm - newm;
        return ptr;
      } else if(a->chunks->left >= newm - oldm) {
        /* There is space to expand */
        a->chunks->ptr += newm - oldm;
        a->chunks->left -= newm - oldm;
        /* 0-fill the new space.  Note that we do this in byte terms (compare
         * above), to deal with the case where the allocation shrinks by (say)
         * a single non-zero byte but then expands again. */
        memset((char *)ptr + oldn, 0, newn - oldn);
        return ptr;
      }
      /* If we get here then we are expanding but there is not enough space to
       * do so in the same chunk */
    } else if(newm == oldm) {
      /* This is not the most recently block but we're not changing its size
       * anyway */
      return ptr;
    }
    /* We have no choice but to allocate new space */
    newptr = sftp_alloc(a, newn);
    memcpy(newptr, ptr, oldn);
    return newptr;
  } else
    /* There was no old allocation, just create a new one the easy way */
    return sftp_alloc(a, newn);
}

void sftp_alloc_destroy(struct allocator *a) {
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
