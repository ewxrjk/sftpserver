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

/** @file serialize.h @brief Request serialization interface */

#ifndef SERIALIZE_H
#define SERIALIZE_H

/** @brief Establish a job's place in the serialization queue
 * @param job Job to establish
 *
 * Called for @ref SSH_FXP_READ and @ref SSH_FXP_WRITE. */
void queue_serializable_job(struct sftpjob *job);

/** @brief Serialize a job
 * @param job Job to serialize
 *
 * Wait until there are no jobs conflicting with @p job that were established
 * before it in the serialization queue.
 */
void serialize(struct sftpjob *job);

/** @brief Remove a job from the serialization queue
 * @param job Job to remove
 *
 * Called when the job is completed.
 */
void serialize_remove_job(struct sftpjob *job);

#endif /* SERIALIZE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
