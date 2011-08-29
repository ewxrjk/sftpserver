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
/** @brief Maximum request size */
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

/** @brief @ref SSH_FXP_INIT stub for use after initialization
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_already_init(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_REMOVE implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_remove(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_RMDIR implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_rmdir(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_SYMLINK implementation
 * @param job Job
 * @return Error code
 *
 * This for protocols 4 and above.
 */
uint32_t sftp_v345_symlink(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_READLINK implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_readlink(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_CLOSE implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_close(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_READ implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_read(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_WRITE implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_write(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_SETSTAT implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_setstat(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_FSETSTAT implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_fsetstat(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_OPENDIR implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_opendir(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_READDIR implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_readdir(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_MKDIR implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_mkdir(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_EXTENDED implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_extended(struct sftpjob *job);

/** @brief Send a filename list as found in an @ref SSH_FXP_NAME response
 * @param job Job
 * @param nnames Number of names
 * @param names Filenames (and attributes) to send
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
void sftp_v456_sendnames(struct sftpjob *job, 
                         int nnames, const struct sftpattr *names);

/** @brief Send file attributes
 * @param job Job
 * @param attrs File attributes
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
void sftp_v456_sendattrs(struct sftpjob *job,
                         const struct sftpattr *attrs);

/** @brief Parse file attributes
 * @param job Job
 * @param attrs Where to put file attributes
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
uint32_t sftp_v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs);

/** @brief Encode a filename for transmission
 * @param job Job
 * @param path Input/output filename
 * @return 0 on success, -1 on error (as per sftp_iconv())
 *
 * This is for protocol 3 only; @c *path is not modified.
 */
int sftp_v3_encode(struct sftpjob *job, char **path);

/** @brief Encode a filename for transmission
 * @param job Job
 * @param path Input/output filename
 * @return 0 on success, -1 on error (as per sftp_iconv())
 *
 * This is for protocols 4 and above; @c *path is replaced with the UTF-8
 * version of the filename.
 */
int sftp_v456_encode(struct sftpjob *job, char **path);

/** @brief Decode a filename
 * @param job Job
 * @param path Input/output filename
 * @return Error code
 *
 * This is for protocols 4 and above; @c *path is translated from UTF-8 to the
 * current multibyte encoding.
 */
uint32_t sftp_v456_decode(struct sftpjob *job, char **path);

/** @brief Generic @ref SSH_FXP_RENAME implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 3 and 4.
 */
uint32_t sftp_v34_rename(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_OPEN implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 3 and 4.
 */
uint32_t sftp_v34_open(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_LSTAT implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
uint32_t sftp_v456_lstat(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_STAT implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
uint32_t sftp_v456_stat(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_FSTAT implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 4 and above.
 */
uint32_t sftp_v456_fstat(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_REALPATH implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 3-5.
 */
uint32_t sftp_v345_realpath(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_OPEN implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 5 and above.
 */
uint32_t sftp_v56_open(struct sftpjob *job);

/** @brief Generic @ref SSH_FXP_RENAME implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocols 5 and above.
 */
uint32_t sftp_v56_rename(struct sftpjob *job);

/** @brief @ref SSH_FXP_REALPATH implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocol 6 only.
 */
uint32_t sftp_v6_realpath(struct sftpjob *job);

/** @brief @ref SSH_FXP_LINK implementation
 * @param job Job
 * @return Error code
 *
 * This is for protocol 6 only.
 */
uint32_t sftp_v6_link(struct sftpjob *job);

/** @brief @c text-seek extension implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_text_seek(struct sftpjob *job);

/** @brief Common code for @ref SSH_FXP_OPEN
 * @param job Job
 * @param path Path to open (should already have been translated)
 * @param desired_access Access required (TODO link to valid bits)
 * @param flags Open flags (TODO link to valid bits)
 * @param attrs Initial attributes for new files
 * @return Error code
 */
uint32_t sftp_generic_open(struct sftpjob *job, const char *path,
                           uint32_t desired_access, uint32_t flags,
                           struct sftpattr *attrs);

/** @brief @c space-available extension implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_space_available(struct sftpjob *job);

/** @brief @c version-select extension implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_v6_version_select(struct sftpjob *job);

/** @brief @c posix-rename@openssh.org extension implementation
 * @param job Job
 * @return Error code
 */
uint32_t sftp_vany_posix_rename(struct sftpjob *job);

/** @brief @c statfs@openssh.org extension implementation
 * @param job Job
 * @return Error code
 */
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
