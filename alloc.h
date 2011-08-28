/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2011 Richard Kettlewell
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

/** @file alloc.h @brief Allocator interface */

#ifndef ALLOC_H
#define ALLOC_H

struct chunk;

/** @brief An allocator
 *
 * This kind of allocator supports quick allocation but does not support
 * freeing of single allocations.  The only way to release resources is to free
 * all allocations.  It is not thread safe.
 */
struct allocator {
  /** @brief Head of linked list of chunks
   *
   * This is the most recently created chunk, or a null pointer.
   */
  struct chunk *chunks;
};

/** @brief Initialize an allocator
 * @param Allocator to initialize
 * @return @p a
 */
struct allocator *sftp_alloc_init(struct allocator *a);

/** @brief Allocate space from an allocator
 * @param a Allocator
 * @param n Number of bytes to allocate
 * @return Pointer to allocated memory
 *
 * If @p n is 0 then the return value is a null pointer.
 * If memory cannot be allocated, the process is terminated.
 *
 * Newly allocate memory is always 0-filled.
 */
void *sftp_alloc(struct allocator *a, size_t n);

/** @brief Expand an allocation within an allocator
 * @param a Allocator
 * @param ptr Allocation to expand, or a null pointer
 * @param oldn Old size in bytes
 * @param newn New size in bytes
 * @return New allocation
 *
 * If @p ptr is a null pointer then @p oldn must be 0.  The new allocation may
 * be the same or different to @p ptr.  If the new allocation is larger then
 * the extra space is 0-filled.
 */
void *sftp_alloc_more(struct allocator *a, void *ptr, size_t oldn, size_t newn);

/** @brief Destroy an allocator
 * @param a Allocator
 *
 * All memory allocated through the allocator is freed.  The allocator remains
 * initialized.
 */
void sftp_alloc_destroy(struct allocator *a);

#endif /* ALLOC_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
