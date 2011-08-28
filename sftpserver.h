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

/** @file sftpserver.h @brief SFTP server parameters and common declarations */

#ifndef SFTPSERVER_H
#define SFTPSERVER_H

#include "sftpcommon.h"

#include <sys/types.h>

#ifndef MAXNAMES
/** @brief Maximum size of an @ref SSH_FXP_READDIR response */
# define MAXNAMES 32
#endif

#ifndef MAXHANDLES
/** @brief Maximum number of concurrent handles */
# define MAXHANDLES 128
#endif

#ifndef MAXREAD
/** @brief Maximum read size */
# define MAXREAD 1048576
#endif

#ifndef MAXREQUEST
/** @brief Maximum request size
 *
 * @todo Needs to be tied in with @ref max_request_len. */
# define MAXREQUEST 1048576
#endif

#ifndef DEFAULT_PERMISSIONS
/** @brief Default file permissions */
# define DEFAULT_PERMISSIONS 0755
#endif

/** @brief Send an @ref SSH_FXP_STATUS message
 * @param job Job
 * @param status Status code
 * @param msg Human-readable message
 */
void sftp_send_status(struct sftpjob *job, 
                      uint32_t status,
                      const char *msg);

uint32_t sftp_vany_already_init(struct sftpjob *job);
uint32_t sftp_vany_remove(struct sftpjob *job);
uint32_t sftp_vany_rmdir(struct sftpjob *job);
uint32_t sftp_v345_symlink(struct sftpjob *job);
uint32_t sftp_vany_readlink(struct sftpjob *job);
uint32_t sftp_vany_close(struct sftpjob *job);
uint32_t sftp_vany_read(struct sftpjob *job);
uint32_t sftp_vany_write(struct sftpjob *job);
uint32_t sftp_vany_setstat(struct sftpjob *job);
uint32_t sftp_vany_fsetstat(struct sftpjob *job);
uint32_t sftp_vany_opendir(struct sftpjob *job);
uint32_t sftp_vany_readdir(struct sftpjob *job);
uint32_t sftp_vany_mkdir(struct sftpjob *job);
uint32_t sftp_vany_extended(struct sftpjob *job);
void sftp_v456_sendnames(struct sftpjob *job, 
                         int nnames, const struct sftpattr *names);
void sftp_v456_sendattrs(struct sftpjob *job,
                         const struct sftpattr *attrs);
uint32_t sftp_v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs);
int sftp_v3_encode(struct sftpjob *job, char **path);
int sftp_v456_encode(struct sftpjob *job, char **path);
uint32_t sftp_v456_decode(struct sftpjob *job, char **path);
uint32_t sftp_v34_rename(struct sftpjob *job);
uint32_t sftp_v34_open(struct sftpjob *job);
uint32_t sftp_v456_lstat(struct sftpjob *job);
uint32_t sftp_v456_stat(struct sftpjob *job);
uint32_t sftp_v456_fstat(struct sftpjob *job);
uint32_t sftp_v345_realpath(struct sftpjob *job);
uint32_t sftp_v56_open(struct sftpjob *job);
uint32_t sftp_v56_rename(struct sftpjob *job);
uint32_t sftp_v6_realpath(struct sftpjob *job);
uint32_t sftp_v6_link(struct sftpjob *job);
uint32_t sftp_vany_text_seek(struct sftpjob *job);
uint32_t sftp_generic_open(struct sftpjob *job, const char *path,
                           uint32_t desired_access, uint32_t flags,
                           struct sftpattr *attrs);
uint32_t sftp_vany_space_available(struct sftpjob *job);
uint32_t sftp_v6_version_select(struct sftpjob *job);
uint32_t sftp_vany_posix_rename(struct sftpjob *job);
uint32_t sftp_vany_statfs(struct sftpjob *job);

/** @brief Send an @ref SSH_FXP_STATUS message based on @c errno
 * @param job Job
 */
void sftp_send_errno_status(struct sftpjob *job);

#endif /* SFTPSERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
