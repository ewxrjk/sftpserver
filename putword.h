/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2011, 2014 Richard Kettlewell
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

/** @file putword.h @brief Macros for storing network-order values */

#ifndef PUTWORD_H
#  define PUTWORD_H

#  include <arpa/inet.h>

#  ifndef ASM
#    define ASM 1
#  endif

/** @brief Unaligned 16-bit network-order store
 * @param where Where to store value
 * @param u Value to store
 */
static inline void put16(void *where, uint16_t u) {
#  if(__i386__ || __amd64__) && __GNUC__ && ASM
  __asm__ volatile("xchg %h[U],%b[U]\n\tmovw %[U],%[WHERE]"
                   : [U] "+Q"(u)
                   : [WHERE] "m"(*(uint16_t *)where)
                   : "memory");
#  else
  uint8_t *ptr = where;
  *ptr++ = (uint8_t)(u >> 8);
  *ptr = (uint8_t)u;
#  endif
}

/** @brief Unaligned 32-bit network-order store
 * @param where Where to store value
 * @param u Value to store
 */
static inline void put32(void *where, uint32_t u) {
#  if(__i386__ || __amd64__) && __GNUC__ && ASM
  __asm__ volatile("bswapl %[U]\n\tmovl %[U],%[WHERE]"
                   : [U] "+r"(u)
                   : [WHERE] "m"(*(uint32_t *)where)
                   : "memory");
#  else
  uint8_t *ptr = where;
  *ptr++ = (uint8_t)(u >> 24);
  *ptr++ = (uint8_t)(u >> 16);
  *ptr++ = (uint8_t)(u >> 8);
  *ptr++ = (uint8_t)(u);
#  endif
}

/** @brief Unaligned 64-bit network-order store
 * @param where Where to store value
 * @param u Value to store
 */
static inline void put64(void *where, uint64_t u) {
#  if __amd64__ && __GNUC__ && ASM
  __asm__ volatile("bswapq %[U]\n\tmovq %[U],%[WHERE]"
                   : [U] "+r"(u)
                   : [WHERE] "m"(*(uint64_t *)where)
                   : "memory");
#  else
  put32(where, u >> 32);
  put32((char *)where + 4, (uint32_t)u);
#  endif
}

/** @brief Unaligned 16-bit network-order fetch
 * @param where Where to fetch from
 * @return Fetched value
 */
static inline uint16_t get16(const void *where) {
#  if(__i386__ || __amd64__) && __GNUC__ && ASM
  uint16_t r;
  __asm__("movw %[WHERE],%[R]\t\nxchg %h[R],%b[R]"
          : [R] "=Q"(r)
          : [WHERE] "m"(*(const uint16_t *)where));
  return r;
#  else
  const uint8_t *ptr = where;
  return (ptr[0] << 8) + ptr[1];
#  endif
}

/** @brief Unaligned 32-bit network-order fetch
 * @param where Where to fetch from
 * @return Fetched value
 */
static inline uint32_t get32(const void *where) {
#  if(__i386__ || __amd64__) && __GNUC__ && ASM
  uint32_t r;
  __asm__("movl %[WHERE],%[R]\n\tbswapl %[R]"
          : [R] "=r"(r)
          : [WHERE] "m"(*(const uint32_t *)where));
  return r;
#  else
  const uint8_t *ptr = where;
  return ((unsigned)ptr[0] << 24) + (ptr[1] << 16) + (ptr[2] << 8) + ptr[3];
#  endif
}

/** @brief Unaligned 64-bit network-order fetch
 * @param where Where to fetch from
 * @return Fetched value
 */
static inline uint64_t get64(const void *where) {
#  if __amd64__ && __GNUC__ && ASM
  uint64_t r;
  __asm__("movq %[WHERE],%[R]\n\tbswapq %[R]"
          : [R] "=r"(r)
          : [WHERE] "m"(*(const uint64_t *)where));
  return r;
#  else
  return ((uint64_t)get32(where) << 32) + get32((const char *)where + 4);
#  endif
}

#endif /* PUTWORD_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
