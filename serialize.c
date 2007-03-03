#include "sftpserver.h"
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

/* Implement serialization requirements as per draft-ietf-secsh-filexfer-02.txt
 * s6, or -13.txt s4.1.  Two points worth noting:
 *
 * 1) The language there is in terms of 'files'.  I interpret it to refer to
 * handles since it's actually impossible in general to determine whether two
 * files (by name, by FD, etc) are really the same file.
 *
 * 2) The language says you "MUST" process requests in the order received but
 * then backs off a bit and allows non-overlapping requests to be re-ordered or
 * parallelized.  I interpret this to mean that there's an unstated 'as-if'
 * model going on, i.e. the requirement is that the file contents or returned
 * data is the same as if you'd done your operations in order, rather than that
 * the actual operation system calls have to happen in a specific order even if
 * more efficient orderings are available that give the same end result.
 *
 * 3) We don't bother serializing SSH_FXP_READDIR.  Yes, this will result in
 * re-ordering but for all the client knows the underlying server OS gives you
 * different orderings too.  The client could detect the difference by
 * comparing ID fields, if we run into one that parallelizes _and_ cares about
 * this then we can revisit the question.
 *
 */

struct sqnode {
  struct sqnode *older;
  struct sftpjob *job;
  struct handleid hid;
  uint64_t offset;
  uint64_t len;
  uint8_t type;
};

static struct sqnode *newest;
static pthread_mutex_t sq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sq_cond = PTHREAD_COND_INITIALIZER;

/* Return true iff two handles match */
static inline int handles_equal(const struct handleid *h1,
                                const struct handleid *h2) {
  return h1->id == h2->id && h1->tag == h2->tag;
}

/* Return true iff two byte ranges overlap */
static int ranges_overlap(const struct sqnode *a, 
                          const struct sqnode *b) {
  if(a->len && b->len) {
    const uint64_t aend = a->offset + a->len - 1;
    const uint64_t bend = b->offset + b->len - 1;
    
    if(aend >= b->offset && aend <= bend) return 1;
    if(bend >= a->offset && bend <= aend) return 1;
  }
  return 0;
}

/* Return true iff operation N modifies the file */
static int write_operation(int n) {
  switch(n) {
  case SSH_FXP_WRITE:
  case SSH_FXP_FSETSTAT:
  case SSH_FXP_FSTAT:
    /* We consider SSH_FXP_FSTAT a write so that it will not be re-ordered with
     * respect to SSH_FXP_READ (and thus return a wrong atime).  This is a
     * hack!  This prevents it being re-ordered with respect to itself, too,
     * but that's not much of a loss. */
    return 1;
  default:
    return 0;
  }
}

void queue_serializable_job(struct sftpjob *job) {
  uint8_t type;
  uint32_t id;
  uint64_t offset, len64;
  uint32_t len;
  struct handleid hid;
  struct sqnode *q;

  job->ptr = job->data;
  job->left = job->len;
  if(parse_uint8(job, &type)
     || parse_uint32(job, &id)
     || parse_handle(job, &hid))
    return;
  if(type == SSH_FXP_READ || type == SSH_FXP_WRITE) {
    if(parse_uint64(job, &offset)
       || parse_uint32(job, &len))
      return;
    len64 = len;
  } else {
    /* Other operations notionally apply to the whole file, which means they
     * are never re-ordered with respect to any write (or at all if they are
     * themselves a write). */
    offset = 0;
    len64 = ~(uint64_t)0;
  }
  ferrcheck(pthread_mutex_lock(&sq_mutex));
  q = xmalloc(sizeof *q);
  q->older = newest;
  q->job = job;
  q->type = type;
  q->hid = hid;
  q->offset = offset;
  q->len = len64;
  newest = q;
  ferrcheck(pthread_mutex_unlock(&sq_mutex));
}

void serialize_on_handle(struct sftpjob *job, 
                         int istext) {
  struct sqnode *q, *oq;

  ferrcheck(pthread_mutex_lock(&sq_mutex));
  for(;;) {
    for(q = newest; q && q->job != job; q = q->older)
      ;
    /* If the job isn't in the queue then we process it straight away. */
    if(!q)
      break;
    /* We've found our position in the queue.  See if there is any request on
     * the same handle which blocks our request.  Note that for binary files we
     * allow reads to overlap reads, but prohibit writes from overlapping
     * anything.  For text handles we serialize all operations regardless. */
    for(oq = q->older; oq; oq = oq->older) {
      if(handles_equal(&q->hid, &oq->hid)
         && (istext
             || ((write_operation(q->type) || write_operation(oq->type))
                 && ranges_overlap(q, oq))))
        break;
    }
    if(!oq)
      break;
    /* We found an blocking request.  Wait for it to be removed. */
    ferrcheck(pthread_cond_wait(&sq_cond, &sq_mutex));
  }
  /* We did not find any overlapping request. */
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
