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
#include "putword.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;

int sftpout = 1;                        /* default is stdout */

#define sftp_send_raw16(u) do {                 \
  put16(&w->buffer[w->bufused], u);             \
  w->bufused += 2;                              \
} while(0)

#define sftp_send_raw32(u) do {                 \
  put32(&w->buffer[w->bufused], u);             \
  w->bufused += 4;                              \
} while(0)

#define sftp_send_raw64(u) do {                 \
  put64(&w->buffer[w->bufused], u);             \
  w->bufused += 8;                              \
} while(0)

void sftp_send_need(struct worker *w, size_t n) {
  assert(w->bufused < 0x80000000);
  if(n > w->bufsize - w->bufused) {
    size_t newsize = w->bufsize ? w->bufsize : 64;
    while(newsize && newsize < w->bufsize + n)
      newsize <<= 1;
    if(!newsize)
      fatal("out of memory");
    w->buffer = xrealloc(w->buffer, w->bufsize = newsize);
  }
}

void sftp_send_begin(struct worker *w) {
  w->bufused = 0;
  sftp_send_uint32(w, 0);                    /* placeholder for length */
}

void sftp_send_end(struct worker *w) {
  ssize_t n, written;

  assert(w->bufused < 0x80000000);
  /* Fill in length word.  The malloc'd area is assumed to be aligned
   * suitably. */
  *(uint32_t *)w->buffer = htonl(w->bufused - 4);
  /* Write the complete output, protecting stdout with a lock to avoid
   * interleaving different responses. */
  ferrcheck(pthread_mutex_lock(&output_lock));
  if(sftp_debugging) {
    D(("%s:", sendtype));
    sftp_debug_hexdump(w->buffer + 4, w->bufused - 4);
  }
  /* Write the whole buffer, coping with short writes */
  written = 0;
  while((size_t)written < w->bufused)
    if((n = write(sftpout, w->buffer + written, w->bufused - written)) > 0)
      written += n;
    else if(n < 0)
      fatal("error sending response: %s", strerror(errno));
  ferrcheck(pthread_mutex_unlock(&output_lock));
  w->bufused = 0x80000000;
}

void sftp_send_uint8(struct worker *w, int n) {
  sftp_send_need(w, 1);
  w->buffer[w->bufused++] = (uint8_t)n;
}

void sftp_send_uint16(struct worker *w, uint16_t u) {
  sftp_send_need(w, 2);
  sftp_send_raw16(u);
}

void sftp_send_uint32(struct worker *w, uint32_t u) {
  sftp_send_need(w, 4);
  sftp_send_raw32(u);
}

void sftp_send_uint64(struct worker *w, uint64_t u) {
  sftp_send_need(w, 8);
  sftp_send_raw64(u);
}

void sftp_send_bytes(struct worker *w, const void *bytes, size_t n) {
  sftp_send_need(w, n + 4);
  sftp_send_raw32(n);
  memcpy(w->buffer + w->bufused, bytes, n);
  w->bufused += n;
}

void sftp_send_string(struct worker *w, const char *s) {
  sftp_send_bytes(w, s, strlen(s));
}

void sftp_send_path(struct sftpjob *job, struct worker *w, const char *path) {
  if(protocol->encode(job, (char **)&path))
    fatal("cannot encode local path name '%s'", path);
  sftp_send_string(w, path);
}

void sftp_send_handle(struct worker *w, const struct handleid *id) {
  sftp_send_need(w, 12);
  sftp_send_raw32(8);
  sftp_send_raw32(id->id);
  sftp_send_raw32(id->tag);
}

size_t sftp_send_sub_begin(struct worker *w) {
  sftp_send_need(w, 4);
  w->bufused += 4;
  return w->bufused;
}

void sftp_send_sub_end(struct worker *w, size_t offset) {
  const size_t latest = w->bufused;
  w->bufused = offset - 4;
  sftp_send_raw32(latest - offset);
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
