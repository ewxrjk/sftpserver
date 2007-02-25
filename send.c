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
  if(n > w->bufsize - w->bufused) {
    size_t newsize = w->bufsize ? w->bufsize : 64;
    while(newsize && newsize < w->bufsize + n)
      newsize <<= 1;
    if(!newsize) fatal("out of memory");
    w->buffer = xrealloc(w->buffer, w->bufsize = newsize);
  }
}

void send_begin(struct sftpjob *job) {
  struct worker *const w = job->worker;

  w->bufused = 0;
  send_uint32(job, 0);                  /* placeholder for length */
}

void send_end(struct sftpjob *job) {
  struct worker *const w = job->worker;
  ssize_t n, written;

  assert(w->bufused < 0x80000000);
  /* Fill in length word.  The malloc'd area is assumed to be aligned
   * suitably. */
  *(uint32_t *)w->buffer = htonl(w->bufused - 4);
  /* Write the complete output, protecting stdout with a lock to avoid
   * interleaving different responses. */
  ferrcheck(pthread_mutex_lock(&output_lock));
  if(DEBUG) {
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

void send_uint8(struct sftpjob *job, int n) {
  struct worker *const w = job->worker;
  
  send_need(w, 1);
  w->buffer[w->bufused++] = (uint8_t)n;
}

void send_uint32(struct sftpjob *job, uint32_t u) {
  struct worker *const w = job->worker;
  
  send_need(w, 4);
  send_raw32(u);
}

void send_uint64(struct sftpjob *job, uint64_t u) {
  struct worker *const w = job->worker;
  
  send_need(w, 4);
  send_raw64(u);
}

void send_bytes(struct sftpjob *job, const void *bytes, size_t n) {
  struct worker *const w = job->worker;
  
  send_need(w, n + 4);
  send_raw32(n);
  memcpy(w->buffer + w->bufused, bytes, n);
  w->bufused += n;
}

void send_path(struct sftpjob *job, const char *path) {
  protocol->encode(job, (char **)&path);
  send_string(job, path);
}

void send_handle(struct sftpjob *job, const struct handleid *id) {
  struct worker *const w = job->worker;
  
  send_need(w, 12);
  send_raw32(8);
  send_raw32(id->id);
  send_raw32(id->tag);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
