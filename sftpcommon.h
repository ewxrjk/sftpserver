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

#ifndef SFTPCOMMON_H
#define SFTPCOMMON_H

#include <config.h>
#include <inttypes.h>
#include <sys/types.h>

#if HAVE_ENDIAN_H
# include <endian.h>
#endif

#if __GNUC__ && __amd64__
#define BSWAP64(N)				\
  ({uint64_t __n = (N); __asm__("bswap %0" : "+q"(__n)); __n;})
#endif

#if __GNUC__ && __i386__
/* This is not entirely satisfactory: the xchgl would be unnecessary, if only
 * we had some way of communicating the detailed input and output assignments
 * of the registers to the compiler. */
#define BSWAP64(N)                                        \
({uint64_t __n = (N); __asm__("xchgl %%eax,%%edx\n"      \
                              "\tbswap %%eax\n"         \
                              "\tbswap %%edx"           \
                              : "+A"(__n));             \
  __n;})
#endif

#if HAVE_DECL_BE64TOH
# define NTOHLL be64toh
#endif
#if HAVE_DECL_HTOBE64
#define HTONLL htobe64
#endif
#if defined BSWAP64 && !defined NTOHLL
# define NTOHLL BSWAP64
#endif
#if defined BSWAP64 && !defined HTONLL
# define HTONLL BSWAP64
#endif

struct queue;
struct allocator;
struct handleid;
struct sftpjob;
struct sftpattr;
struct worker;
struct stat;

const char *status_to_string(uint32_t status);

#endif /* SFTPCOMMON_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
