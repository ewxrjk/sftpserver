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
#include "debug.h"
#include "utils.h"
#include "handle.h"
#include "send.h"
#include "thread.h"
#include "types.h"
#include "globals.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;

int sftpout = 1;                        /* default is stdout */

#if UNALIGNED_WRITES
# define send_raw32(u) do {                             \
  *(uint32_t *)&w->buffer[w->bufused] = htonl(u);       \
  w->bufused += 4;                                      \
} while(0)
# define send_raw16(u) do {                             \
  *(uint16_t *)&w->buffer[w->bufused] = htons(u);       \
  w->bufused += 2;                                      \
} while(0)
#endif

#ifndef send_raw16
# define send_raw16(u) do {                             \
  const uint16_t uu = (uint16_t)(u);                    \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 8);         \
  w->buffer[w->bufused++] = (uint8_t)(uu);              \
} while(0)
#endif

#ifndef send_raw32
# define send_raw32(u) do {                             \
  const uint32_t uu = (uint32_t)(u);                    \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 24)         \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 16);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 8);         \
  w->buffer[w->bufused++] = (uint8_t)(uu);              \
} while(0)
#endif

#ifndef send_raw64
# define send_raw64(u) do {                             \
  const uint64_t uu = u;                                \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 56);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 48);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 40);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 32);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 24);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 16);        \
  w->buffer[w->bufused++] = (uint8_t)(uu >> 8);         \
  w->buffer[w->bufused++] = (uint8_t)(uu);              \
} while(0)
#endif

void send_need(struct worker *w, size_t n) {
  assert(w->bufused < 0x80000000);
  if(n > w->bufsize - w->bufused) {
    size_t newsize = w->bufsize ? w->bufsize : 64;
    while(newsize && newsize < w->bufsize + n)
      newsize <<= 1;
    if(!newsize) fatal("out of memory");
    w->buffer = xrealloc(w->buffer, w->bufsize = newsize);
  }
}

void send_begin(struct worker *w) {
  w->bufused = 0;
  send_uint32(w, 0);                    /* placeholder for length */
}

void send_end(struct worker *w) {
  ssize_t n, written;

  assert(w->bufused < 0x80000000);
  /* Fill in length word.  The malloc'd area is assumed to be aligned
   * suitably. */
  *(uint32_t *)w->buffer = htonl(w->bufused - 4);
  /* Write the complete output, protecting stdout with a lock to avoid
   * interleaving different responses. */
  ferrcheck(pthread_mutex_lock(&output_lock));
  if(debugging) {
    D(("%s:", sendtype));
    hexdump(w->buffer + 4, w->bufused - 4);
  }
  /* Write the whole buffer, coping with short writes */
  written = 0;
  while((n = write(sftpout, w->buffer + written, w->bufused - written)) > 0)
    written += n;
  if(n < 0)
    fatal("error sending response: %s", strerror(errno));
  ferrcheck(pthread_mutex_unlock(&output_lock));
  w->bufused = 0x80000000;
}

void send_uint8(struct worker *w, int n) {
  send_need(w, 1);
  w->buffer[w->bufused++] = (uint8_t)n;
}

void send_uint16(struct worker *w, uint16_t u) {
  send_need(w, 2);
  send_raw16(u);
}

void send_uint32(struct worker *w, uint32_t u) {
  send_need(w, 4);
  send_raw32(u);
}

void send_uint64(struct worker *w, uint64_t u) {
  send_need(w, 8);
  send_raw64(u);
}

void send_bytes(struct worker *w, const void *bytes, size_t n) {
  send_need(w, n + 4);
  send_raw32(n);
  memcpy(w->buffer + w->bufused, bytes, n);
  w->bufused += n;
}

void send_path(struct sftpjob *job, struct worker *w, const char *path) {
  if(protocol->encode(job, (char **)&path))
    fatal("cannot encode local path name '%s'", path);
  send_string(w, path);
}

void send_handle(struct worker *w, const struct handleid *id) {
  send_need(w, 12);
  send_raw32(8);
  send_raw32(id->id);
  send_raw32(id->tag);
}

size_t send_sub_begin(struct worker *w) {
  send_need(w, 4);
  w->bufused += 4;
  return w->bufused;
}

void send_sub_end(struct worker *w, size_t offset) {
  const size_t latest = w->bufused;
  w->bufused = offset - 4;
  send_raw32(latest - offset);
  w->bufused = latest;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
