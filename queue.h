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

/** @file queue.h @brief Thread pool/queue interface */

#ifndef QUEUE_H
#define QUEUE_H

struct allocator;

/** @brief Queue-specific callbacks */
struct queuedetails {
  /** @brief Per-thread initialization
   * @return Per-thread value to be passed to other callbacks
   */
  void *(*init)();

  /** @brief Worker method
   * @param job Job to process
   * @param workerdata Per-thread value from @c init()
   * @param a Per-thread allocator to use for any memory requirements
   */
  void (*worker)(void *job, void *workerdata, struct allocator *a);

  /** @brief Per-thread cleanup
   * @param workerdata Per-thread value from @c init()
   */
  void (*cleanup)(void *workerdata);
};

/** @brief Create a thread pool and queue
 * @param qp Where store queue pointer
 * @param details Queue-specific callbacks (not copied)
 * @param nthreads Number of threads to create
 */
void queue_init(struct queue **qp,
		const struct queuedetails *details,
		int nthreads);

/** @brief Add a job to a thread pool's queue
 * @param q Queue pointer
 * @param job Job to add to queue
 *
 * Add a JOB to Q.  Jobs are executed in order but if NTHREADS>1 then
 * their processing may overlap in time.  On non-threaded systems, the
 * job is executed before returning from queue_add(). */
void queue_add(struct queue *q, void *job);

/** @brief Destroy a queue
 * @param q Queue pointer
 *
 * All unprocessed jobs are executed before completion. */
void queue_destroy(struct queue *q);

#endif /* QUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
