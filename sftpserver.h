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

#ifndef SFTPSERVER_H
#define SFTPSERVER_H

#include "sftpcommon.h"

#include <sys/types.h>

/* Maximum numbers in a response to SSH_FXP_READDIR */
#ifndef MAXNAMES
# define MAXNAMES 32
#endif

/* Maximum number of concurrent handles supported */
#ifndef MAXHANDLES
# define MAXHANDLES 128
#endif

/* Maximum read size */
#ifndef MAXREAD
# define MAXREAD 1048576
#endif

/* Maximum request size (NOT IMPLEMENTED!) */
#ifndef MAXREQUEST
# define MAXREQUEST 1048576
#endif

/* Default file permissions */
#ifndef DEFAULT_PERMISSIONS
# define DEFAULT_PERMISSIONS 0755
#endif

void send_status(struct sftpjob *job, 
                 uint32_t status,
                 const char *msg);
/* Send an SSH_FXP_STATUS */

uint32_t sftp_already_init(struct sftpjob *job);
uint32_t sftp_remove(struct sftpjob *job);
uint32_t sftp_rmdir(struct sftpjob *job);
uint32_t sftp_symlink(struct sftpjob *job);
uint32_t sftp_readlink(struct sftpjob *job);
uint32_t sftp_close(struct sftpjob *job);
uint32_t sftp_read(struct sftpjob *job);
uint32_t sftp_write(struct sftpjob *job);
uint32_t sftp_setstat(struct sftpjob *job);
uint32_t sftp_fsetstat(struct sftpjob *job);
uint32_t sftp_opendir(struct sftpjob *job);
uint32_t sftp_readdir(struct sftpjob *job);
uint32_t sftp_mkdir(struct sftpjob *job);
uint32_t sftp_extended(struct sftpjob *job);

void v456_sendnames(struct sftpjob *job, 
                    int nnames, const struct sftpattr *names);
void v456_sendattrs(struct sftpjob *job,
 		    const struct sftpattr *attrs);
int v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs);
int v456_encode(struct sftpjob *job, char **path);
int v456_decode(struct sftpjob *job, char **path);
uint32_t sftp_v34_rename(struct sftpjob *job);
uint32_t sftp_v34_open(struct sftpjob *job);
uint32_t sftp_v456_lstat(struct sftpjob *job);
uint32_t sftp_v456_stat(struct sftpjob *job);
uint32_t sftp_v456_fstat(struct sftpjob *job);
uint32_t sftp_v345_realpath(struct sftpjob *job);
uint32_t sftp_v56_open(struct sftpjob *job);
uint32_t sftp_v56_rename(struct sftpjob *job);
uint32_t sftp_v6_realpath(struct sftpjob *job);
uint32_t sftp_link(struct sftpjob *job);
uint32_t sftp_text_seek(struct sftpjob *job);
uint32_t generic_open(struct sftpjob *job, const char *path,
                  uint32_t desired_access, uint32_t flags,
                  struct sftpattr *attrs);
uint32_t sftp_space_available(struct sftpjob *job);

void send_errno_status(struct sftpjob *job);
/* Call send_status based on errno */

#endif /* SFTPSERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
