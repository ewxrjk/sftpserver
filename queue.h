#ifndef QUEUE_H
#define QUEUE_H

struct allocator;

struct queuedetails {
  void *(*init)();                      /* returns workerdata */
  void (*worker)(void *job, void *workerdata, struct allocator *a);
  void (*cleanup)(void *workerdata);
};

void queue_init(struct queue **qp,
		const struct queuedetails *details,
		int nthreads);
/* Create a new queue with up to NTHREADS threads.  All jobs will be
 * executed via WORKER with a private allocator. */

void queue_add(struct queue *q, void *job);
/* Add a JOB to Q.  Jobs are executed in order but if NTHREADS>1 then
 * their processing may overlap in time.  On non-threaded systems, the
 * job is executed before returning from queue_add(). */

void queue_destroy(struct queue *q);
/* Destroy Q.  All unprocess jobs are executed before completion. */

#endif /* QUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
