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

#ifndef PARSE_H
#define PARSE_H

uint32_t sftp_parse_uint8(struct sftpjob *job, uint8_t *ur);
uint32_t sftp_parse_uint16(struct sftpjob *job, uint16_t *ur);
uint32_t sftp_parse_uint32(struct sftpjob *job, uint32_t *ur);
uint32_t sftp_parse_uint64(struct sftpjob *job, uint64_t *ur);
uint32_t sftp_parse_string(struct sftpjob *job, char **strp, size_t *lenp);
uint32_t sftp_parse_path(struct sftpjob *job, char **strp);
uint32_t sftp_parse_handle(struct sftpjob *job, struct handleid *id);
/* Parse various values out of the remaining data of a job.  Return 0 on
 * success, non-0 on error. */

#if CLIENT
#define cpcheck(E) do {                                                 \
  const uint32_t rc = (E);                                              \
  if(rc) {                                                              \
    D(("%s:%d: %s returned %"PRIu32, __FILE__, __LINE__, #E, rc));      \
    fatal("error parsing response from server");                        \
  }                                                                     \
} while(0)
#else
#define pcheck(E) do {                                          \
  const uint32_t rc = (E);                                      \
  if(rc != SSH_FX_OK) {                                         \
    D(("%s:%d: %s: %"PRIu32, __FILE__, __LINE__, #E, rc));      \
    return rc;                                                  \
  }                                                             \
} while(0)
/* error-checking wrapper for sftp_parse_ functions */
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
