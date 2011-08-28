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

/** @file send.h @brief Message sending interface */

#ifndef SEND_H
#define SEND_H

/** @brief Ensure there are @p n bytes spare in the message buffer
 * @param w Worker containing message
 * @param n Bytes required
 *
 * If memory cannot be allocated the process is terminated.
 */
void sftp_send_need(struct worker *w, size_t n);

/** @brief Start to construct a message
 * @param w Worker containing message
 *
 * Any previous or partial message is discarded.
 */
void sftp_send_begin(struct worker *w);

/** @brief Complete a message
 * @param w Worker containing message
 *
 * The message is sent to @ref sftpout.  All sends are serialized so if two
 * messages are sent concurrently, they are not interleaved.
 *
 * After this call the message cannot be further modified; sftp_send_begin()
 * must be called again.  Failure to do so will trigger an assert.
 */
void sftp_send_end(struct worker *w);

/** @brief Add an 8-bit value to a message
 * @param w Worker containing message
 * @param n Value to add to message
 */
void sftp_send_uint8(struct worker *w, int n);

/** @brief Add a 16-bit value to a message
 * @param w Worker containing message
 * @param u Value to add to message
 */
void sftp_send_uint16(struct worker *w, uint16_t u);

/** @brief Add a 32-bit value to a message
 * @param w Worker containing message
 * @param u Value to add to message
 */
void sftp_send_uint32(struct worker *w, uint32_t u);

/** @brief Add a 64-bit value to a message
 * @param w Worker containing message
 * @param u Value to add to message
 */
void sftp_send_uint64(struct worker *w, uint64_t u);

/** @brief Add a byte block to a message
 * @param w Worker containing message
 * @param bytes Start of byte block
 * @param n Size of byte block
 */
void sftp_send_bytes(struct worker *w, const void *bytes, size_t n);

/** @brief Add a handle a message
 * @param w Worker containing message
 * @param id Handle to add to message
 */
void sftp_send_handle(struct worker *w, const struct handleid *id);

/** @brief Add a string a message
 * @param w Worker containing message
 * @param s String to add to message
 */
void sftp_send_string(struct worker *w, const char *s);

/** @brief Add a path a message
 * @param job Job to borrow allocator from
 * @param w Worker containing message
 * @param path Path to add to message
 */
void sftp_send_path(struct sftpjob *job, struct worker *w, const char *path);

/** @brief Start a sub-message
 * @param w Worker containing message
 * @return Offset to pass to sftp_send_sub_end()
 *
 * Sub-messages have their own initial length word.
 */
size_t sftp_send_sub_begin(struct worker *w);

/** @brief Complete a sub-message
 * @param w Worker containing message
 * @param offset Return value of previous sftp_send_sub_begin()
 */
void sftp_send_sub_end(struct worker *w, size_t offset);

/** @brief File descriptor to send messages to
 *
 * The default value is 1, i.e. standard output. */
extern int sftpout;

#endif /* SEND_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
