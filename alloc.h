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

#ifndef ALLOC_H
#define ALLOC_H

struct chunk;

struct allocator {
  struct chunk *chunks;
};

struct allocator *sftp_alloc_init(struct allocator *a);
/* Initialize allocator A */

void *sftp_alloc(struct allocator *a, size_t n);
/* Allocate N bytes from A.  The new space is 0-filled. */

void *sftp_alloc_more(struct allocator *a, void *ptr, size_t oldn, size_t newn);
/* Expand from OLDN bytes to NEWN bytes.  May move the contents. */

void sftp_alloc_destroy(struct allocator *a);
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
