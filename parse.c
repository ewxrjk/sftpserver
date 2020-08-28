/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2011, 2014 Richard Kettlewell
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
#include "alloc.h"
#include "debug.h"
#include "handle.h"
#include "parse.h"
#include "types.h"
#include "sftp.h"
#include "globals.h"
#include "putword.h"
#include <string.h>

uint32_t sftp_parse_uint8(struct sftpjob *job, uint8_t *ur) {
  if(job->left < 1)
    return SSH_FX_BAD_MESSAGE;
  *ur = *job->ptr++;
  --job->left;
  return SSH_FX_OK;
}

uint32_t sftp_parse_uint16(struct sftpjob *job, uint16_t *ur) {
  if(job->left < 2)
    return SSH_FX_BAD_MESSAGE;
  *ur = get16(job->ptr);
  job->ptr += 2;
  job->left -= 2;
  return SSH_FX_OK;
}

uint32_t sftp_parse_uint32(struct sftpjob *job, uint32_t *ur) {
  if(job->left < 4)
    return SSH_FX_BAD_MESSAGE;
  *ur = get32(job->ptr);
  job->ptr += 4;
  job->left -= 4;
  return SSH_FX_OK;
}

uint32_t sftp_parse_uint64(struct sftpjob *job, uint64_t *ur) {
  if(job->left < 8)
    return SSH_FX_BAD_MESSAGE;
  *ur = get64(job->ptr);
  job->ptr += 8;
  job->left -= 8;
  return SSH_FX_OK;
}

uint32_t sftp_parse_string(struct sftpjob *job, char **strp, size_t *lenp) {
  uint32_t len, rc;
  char *str;

  if((rc = sftp_parse_uint32(job, &len)) != SSH_FX_OK)
    return rc;
  if(len == 0xFFFFFFFF)
    return SSH_FX_BAD_MESSAGE; /* overflow */
  if(job->left < len)
    return SSH_FX_BAD_MESSAGE; /* not enough bytes to satisfy */
  if(lenp)
    *lenp = len;
  if(strp) {
    str = sftp_alloc(job->a, len + 1); /* 0-fills */
    memcpy(str, job->ptr, len);
    *strp = str;
  }
  job->ptr += len;
  job->left -= len;
  return SSH_FX_OK;
}

uint32_t sftp_parse_path(struct sftpjob *job, char **strp) {
  uint32_t rc;

  if((rc = sftp_parse_string(job, strp, 0)) != SSH_FX_OK)
    return rc;
  return protocol->decode(job, strp);
}

uint32_t sftp_parse_handle(struct sftpjob *job, struct handleid *id) {
  uint32_t len, rc;

  if((rc = sftp_parse_uint32(job, &len)) != SSH_FX_OK)
    return rc;
  if(len != 8)
    return SSH_FX_BAD_MESSAGE;
  if((rc = sftp_parse_uint32(job, &id->id)) != SSH_FX_OK ||
     (rc = sftp_parse_uint32(job, &id->tag) != SSH_FX_OK))
    return rc;
  return SSH_FX_OK;
}

/*
LOCAL Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
