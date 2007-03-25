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

#ifndef SEND_H
#define SEND_H

void sftp_send_need(struct worker *w, size_t n);
/* Make sure there are N bytes available in the buffer */

void sftp_send_begin(struct worker *w);
void sftp_send_end(struct worker *w);
void sftp_send_uint8(struct worker *w, int n);
void sftp_send_uint16(struct worker *w, uint16_t u);
void sftp_send_uint32(struct worker *w, uint32_t u);
void sftp_send_uint64(struct worker *w, uint64_t u);
void sftp_send_bytes(struct worker *w, const void *bytes, size_t n);
void sftp_send_handle(struct worker *w, const struct handleid *id);
void sftp_send_string(struct worker *w, const char *s);
void sftp_send_path(struct sftpjob *job, struct worker *w, const char *path);

size_t sftp_send_sub_begin(struct worker *w);
void sftp_send_sub_end(struct worker *w, size_t offset);

extern int sftpout;                     /* fd to write to */

#endif /* SEND_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
