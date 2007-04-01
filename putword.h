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

#ifndef PUTWORD_H
#define PUTWORD_H

#if UNALIGNED_WRITES
# define put32(where, u) do {			\
  *(uint32_t *)(where) = htonl(u);		\
} while(0)
# define put16(where, u) do {			\
  *(uint16_t *)(where) = htons(u);		\
} while(0)
#endif

#ifndef put16
# define put16(where, u) do {                   \
  uint8_t *ptr = (void *)(where);               \
  const uint16_t uu = (uint16_t)(u);            \
  *ptr++ = (uint8_t)(uu >> 8);                   \
  *ptr = (uint8_t)(uu);                         \
} while(0)
#endif

#ifndef put32
# define put32(where, u) do {                   \
  const uint32_t uu = (uint32_t)(u);            \
  uint8_t *ptr = (void *)(where);               \
  *ptr++ = (uint8_t)(uu >> 24);                 \
  *ptr++ = (uint8_t)(uu >> 16);                 \
  *ptr++ = (uint8_t)(uu >> 8);                  \
  *ptr++ = (uint8_t)(uu);                       \
} while(0)
#endif

#ifndef sftp_send_raw64
# define put64(where, u) do {                   \
  const uint64_t uu = u;                        \
  uint8_t *ptr = (void *)(where);               \
  *ptr++ = (uint8_t)(uu >> 56);                 \
  *ptr++ = (uint8_t)(uu >> 48);                 \
  *ptr++ = (uint8_t)(uu >> 40);                 \
  *ptr++ = (uint8_t)(uu >> 32);                 \
  *ptr++ = (uint8_t)(uu >> 24);                 \
  *ptr++ = (uint8_t)(uu >> 16);                 \
  *ptr++ = (uint8_t)(uu >> 8);                  \
  *ptr++ = (uint8_t)(uu);                       \
} while(0)
#endif

#endif /* PUTWORD_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
