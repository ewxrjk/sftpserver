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

#include "queue.h"
#include "alloc.h"
#include "debug.h"
#include "utils.h"
#include "thread.h"
#include <stdlib.h>
#include <string.h>

struct queuejob {
  struct queuejob *next;
  void *job;
};

struct queue {
  struct queuejob *jobs, **jobstail;
  pthread_mutex_t m;
  pthread_cond_t c;
  const struct queuedetails *details;
  int nthreads;
  pthread_t *threads;
  int join;
};

/* Worker thread */
static void *queue_thread(void *vq) {
  struct queue *const q = vq;
  struct queuejob *qj;
  struct allocator a;
  void *workerdata;

  workerdata = q->details->init();
  ferrcheck(pthread_mutex_lock(&q->m));
  while(q->jobs || !q->join) {
    if(q->jobs) {
      /* Pick job of front of queue */
      qj = q->jobs;
      if(!(q->jobs = qj->next))
        q->jobstail = &q->jobs;
      /* Don't hold lock while executing job */
      ferrcheck(pthread_mutex_unlock(&q->m));
      sftp_alloc_init(&a);
      q->details->worker(qj->job, workerdata, &a);
      sftp_alloc_destroy(&a);
      free(qj);
      ferrcheck(pthread_mutex_lock(&q->m));
    } else {
      /* Nothing's happening, wait for a signal */
      ferrcheck(pthread_cond_wait(&q->c, &q->m));
    }
  }
  ferrcheck(pthread_mutex_unlock(&q->m));
  q->details->cleanup(workerdata);
  return 0;
}

void queue_init(struct queue **qr,
		const struct queuedetails *details,
		int nthreads) {
  int n;
  struct queue *q;

  q = xmalloc(sizeof *q);
  memset(q, 0, sizeof *q);
  q->jobs = 0;
  q->jobstail = &q->jobs;
  ferrcheck(pthread_mutex_init(&q->m, 0));
  ferrcheck(pthread_cond_init(&q->c, 0));
  q->details = details;
  q->nthreads = nthreads;
  q->threads = xcalloc(nthreads, sizeof (pthread_t));
  q->join = 0;
  for(n = 0; n < q->nthreads; ++n)
    ferrcheck(pthread_create(&q->threads[n], 0, queue_thread, q));
  *qr = q;
}

void queue_add(struct queue *q, void *job) {
  struct queuejob *qj;

  qj = xmalloc(sizeof *qj);
  qj->next = 0;
  qj->job = job;
  ferrcheck(pthread_mutex_lock(&q->m));
  *q->jobstail = qj;
  q->jobstail = &qj->next;
  ferrcheck(pthread_cond_signal(&q->c)); /* any one thread */
  ferrcheck(pthread_mutex_unlock(&q->m));
}

void queue_destroy(struct queue *q) {
  int n;
  
  if(q) {
    ferrcheck(pthread_mutex_lock(&q->m));
    q->join = 1;
    ferrcheck(pthread_cond_broadcast(&q->c)); /* all threads */
    ferrcheck(pthread_mutex_unlock(&q->m));
    for(n = 0; n < q->nthreads; ++n)
      ferrcheck(pthread_join(q->threads[n], 0));
    free(q->threads);
    free(q);
  }
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
