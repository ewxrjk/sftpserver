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

/** @file parse.h @brief Message parsing interface */

#ifndef PARSE_H
#define PARSE_H

/** @brief Retrieve the next byte from a message
 * @param job Job containing message
 * @param ur Where to store value
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 */
uint32_t sftp_parse_uint8(struct sftpjob *job, uint8_t *ur);

/** @brief Retrieve the next 16-bit value from a message
 * @param job Job containing message
 * @param ur Where to store value
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 */
uint32_t sftp_parse_uint16(struct sftpjob *job, uint16_t *ur);

/** @brief Retrieve the next 32-bit value from a message
 * @param job Job containing message
 * @param ur Where to store value
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 */
uint32_t sftp_parse_uint32(struct sftpjob *job, uint32_t *ur);

/** @brief Retrieve the next 64-bit value from a message
 * @param job Job containing message
 * @param ur Where to store byte value
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 */
uint32_t sftp_parse_uint64(struct sftpjob *job, uint64_t *ur);

/** @brief Retrieve the next string value from a message
 * @param job Job containing message
 * @param strp Where to store string
 * @param lenp Where to store length
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 *
 * The string will be allocated using the job's allocator and will not be
 * 0-terminated.
 *
 * @todo Could we just point into the message?
 */
uint32_t sftp_parse_string(struct sftpjob *job, char **strp, size_t *lenp);

/** @brief Retrieve the next path value from a message
 * @param job Job containing message
 * @param strp Where to store path
 * @return 0 on success, error code on error
 *
 * The path will be allocated using the job's allocator and will be
 * 0-terminated.  For protocols other than v3 it will be converted from UTF-8
 * to the current locale's encoding.
 */
uint32_t sftp_parse_path(struct sftpjob *job, char **strp);

/** @brief Retrieve the next handle value from a message
 * @param job Job containing message
 * @param ur Where to store byte value
 * @return 0 on success, @ref SSH_FXP_BAD_MESSAGE on error
 *
 * No attempt is made to validate the handle
 */
uint32_t sftp_parse_handle(struct sftpjob *job, struct handleid *id);

#if CLIENT
/** @brief Error checking wrapper for sftp_parse_... functions
 * @param E expression to check
 *
 * If the parse fails, @ref fatal() is called.  This macro is only used in the
 * client.
 */
#define cpcheck(E) do {                                                 \
  const uint32_t rc = (E);                                              \
  if(rc) {                                                              \
    D(("%s:%d: %s returned %"PRIu32, __FILE__, __LINE__, #E, rc));      \
    fatal("error parsing response from server");                        \
  }                                                                     \
} while(0)
#else
/** @brief Error checking wrapper for sftp_parse_... functions
 * @param E expression to check
 *
 * If the parse fails, @c return is invoked with the error code.
 */
#define pcheck(E) do {                                          \
  const uint32_t rc = (E);                                      \
  if(rc != SSH_FX_OK) {                                         \
    D(("%s:%d: %s: %"PRIu32, __FILE__, __LINE__, #E, rc));      \
    return rc;                                                  \
  }                                                             \
} while(0)
#endif

#endif /* PARSE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
