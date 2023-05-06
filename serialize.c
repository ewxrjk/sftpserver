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

#include "sftpserver.h"
#include "sftpconf.h"
#include "types.h"
#include "thread.h"
#include "utils.h"
#include "handle.h"
#include "sftp.h"
#include "parse.h"
#include "serialize.h"
#include "debug.h"
#include "globals.h"
#include <string.h>
#include <stdlib.h>

/** @file serialize.c @brief Request serialization implementation
 *
 * Implement serialization requirements as per draft-ietf-secsh-filexfer-02.txt
 * s6, or -13.txt s4.1.  Two points worth noting:
 *
 * 1) The language there is in terms of 'files'.  That implies that there's
 * more than just a requirement on (potentially) overlapping reads and writes.
 *
 * 2) The language says you "MUST" process requests in the order received but
 * then backs off a bit and allows non-overlapping requests to be re-ordered or
 * parallelized.  I interpret this to mean that there's an unstated 'as-if'
 * model going on, i.e. the requirement is that the file contents or returned
 * data is the same as if you'd done your operations in order, rather than that
 * the actual operation system calls have to happen in a specific order even if
 * more efficient orderings are available that give the same end result.
 *
 * 3) We are willing to re-order read and write requests (up to a point) but we
 * don't re-order opens, deletes, renames, etc.  So you can stack up an open
 * and a delete and rely on the order in which they are executed.  We don't try
 * to be clever and allow re-ordering of operations where different orders
 * couldn't make a difference.
 *
 * 4) Reads don't include @ref SSH_FXP_READDIR.  Therefore such requests are
 * always serialized, which saves us having to worry about the thread safety of
 * the <dirent.h> functions.  Indeed you could say this about any other
 * operation but readdir() is the most obvious one to worry about.
 *
 */

/** @brief One job in the serialization queue */
struct sqnode {
  /** @brief The next job in the queue */
  struct sqnode *older;

  /** @brief This job */
  struct sftpjob *job;

  /** @brief Handle used by this job */
  struct handleid hid;

  /** @brief Flags for the handle */
  unsigned handleflags;

  /** @brief Offset of the read or write */
  uint64_t offset;

  /** @brief Length of the read or write */
  uint64_t len;

  /** @brief Request type */
  uint8_t type;
};

/** @brief The newest job in the queue */
static struct sqnode *newest;

/** @brief Lock protecting the serialization queue */
static pthread_mutex_t sq_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief Condition variable signaling changes to the queue */
static pthread_cond_t sq_cond = PTHREAD_COND_INITIALIZER;

/** @brief Test whether two handles are identical
 * @param h1 Handle
 * @param h2 Handle
 * @return Nonzero if @p h1 matches @p h2
 */
static inline int handles_equal(const struct handleid *h1,
                                const struct handleid *h2) {
  return h1->id == h2->id && h1->tag == h2->tag;
}

/** @brief Test whether two IO ranges overlap
 * @param a Serialization queue entry
 * @param b Serialization queue entry
 * @return Nonzero if the IO ranges for @p a and @p b overlap
 */
static int ranges_overlap(const struct sqnode *a, const struct sqnode *b) {
  if(a->len && b->len) {
    const uint64_t aend = a->offset + a->len - 1;
    const uint64_t bend = b->offset + b->len - 1;

    if(aend >= b->offset && aend <= bend)
      return 1;
    if(bend >= a->offset && bend <= aend)
      return 1;
  }
  return 0;
}

/** @brief Test wether two jobs may be re-ordered
 * @param q1 Serialization queue entry
 * @param q2 Serialization queue entry
 * @param flags Flags for @p q1
 * @return Nonzero if the @p q1 and @p q2 may be re-ordered
 *
 * @todo Reordering is currently partially disabled because it breaks Paramiko.
 * See https://github.com/robey/paramiko/issues/34 for details.
 */
static int reorderable(const struct sqnode *q1, const struct sqnode *q2,
                       unsigned flags) {
  // Re-ordering can be globally suppressed
  if(!sftpconf_reorder)
    return 0;
  if((q1->type == SSH_FXP_READ || q1->type == SSH_FXP_WRITE) &&
     (q2->type == SSH_FXP_READ || q2->type == SSH_FXP_WRITE)) {
    /* We allow reads and writes to be re-ordered up to a point */
    if(!handles_equal(&q1->hid, &q2->hid)) {
      /* Operations on different handles can always be re-ordered. */
      return 1;
    }
    /* Paramiko's prefetch algorithm assumes that response order matches
     * request order.  As a workaround we avoid re-ordering reads until a fix
     * is adequately widely deployed ("in Debian stable" seems like a good
     * measure). */
    if(q1->type == SSH_FXP_READ && q2->type == SSH_FXP_READ)
      return 0;
    if(flags & (HANDLE_TEXT | HANDLE_APPEND))
      /* Operations on text or append-write files cannot be re-ordered. */
      return 0;
    if(q1->type == SSH_FXP_WRITE || q2->type == SSH_FXP_WRITE)
      if(ranges_overlap(q1, q2))
        /* If one of the operations is a write and the ranges overlap then no
         * re-ordering is allowed. */
        return 0;
    return 1;
  } else
    /* Nothing else may be re-ordered with respect to anything */
    return 0;
}

void queue_serializable_job(struct sftpjob *job) {
  uint8_t type;
  uint32_t id;
  uint64_t offset, len64;
  uint32_t len;
  struct handleid hid;
  unsigned handleflags;
  struct sqnode *q;

  job->ptr = job->data;
  job->left = job->len;
  type = 0; // First sftp_parse_uint8 can't fail, but GCC LTO doesn't know this
            // and moans
  if(!sftp_parse_uint8(job, &type) &&
     (type == SSH_FXP_READ || type == SSH_FXP_WRITE) &&
     sftp_parse_uint32(job, &id) == SSH_FX_OK &&
     sftp_parse_handle(job, &hid) == SSH_FX_OK &&
     sftp_parse_uint64(job, &offset) == SSH_FX_OK &&
     sftp_parse_uint32(job, &len) == SSH_FX_OK) {
    /* This is a well-formed read or write operation */
    len64 = len;
    handleflags = sftp_handle_flags(&hid);
  } else {
    /* Anything else has dummy values */
    sftp_memset(&hid, 0, sizeof hid);
    offset = 0;
    len64 = ~(uint64_t)0;
    handleflags = 0;
  }
  ferrcheck(pthread_mutex_lock(&sq_mutex));
  q = sftp_xmalloc(sizeof *q);
  q->older = newest;
  q->job = job;
  q->type = type;
  q->hid = hid;
  q->handleflags = handleflags;
  q->offset = offset;
  q->len = len64;
  newest = q;
  ferrcheck(pthread_mutex_unlock(&sq_mutex));
}

void serialize(struct sftpjob *job) {
  struct sqnode *q, *oq;

  ferrcheck(pthread_mutex_lock(&sq_mutex));
  for(;;) {
    for(q = newest; q && q->job != job; q = q->older)
      ;
    /* If the job isn't in the queue then we process it straight away.  This
     * shouldn't happen... */
    if(!q)
      break;
    /* We've found our position in the queue.  See if there is any request on
     * the same handle which blocks our request. */
    for(oq = q->older; oq; oq = oq->older)
      if(!reorderable(q, oq, q->handleflags))
        break;
    if(!oq)
      break;
    /* We found an blocking request.  Wait for it to be removed. */
    ferrcheck(pthread_cond_wait(&sq_cond, &sq_mutex));
  }
  /* We did not find any blocking request.  We proceed. */
  ferrcheck(pthread_mutex_unlock(&sq_mutex));
}

void serialize_remove_job(struct sftpjob *job) {
  struct sqnode *q, **qq;

  ferrcheck(pthread_mutex_lock(&sq_mutex));
  for(qq = &newest; (q = *qq) && q->job != job; qq = &q->older)
    ;
  if(q) {
    *qq = q->older;
    free(q);
    /* Wake up anything that's waiting */
    ferrcheck(pthread_cond_broadcast(&sq_cond));
  }
  ferrcheck(pthread_mutex_unlock(&sq_mutex));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
