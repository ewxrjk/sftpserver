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

/** @file globals.h @brief Global variables interface */

#ifndef GLOBALS_H
#define GLOBALS_H

#if NTHREADS > 1
/** @brief Work queue for the thread pool */
extern struct queue *workqueue;
#else
# define workqueue 0
#endif

/** @brief V6 protocol callbacks */
extern const struct sftpprotocol sftp_v6;

/** @brief V5 protocol callbacks */
extern const struct sftpprotocol sftp_v5;

/** @brief V4 protocol callbacks */
extern const struct sftpprotocol sftp_v4;

/** @brief V3 protocol callbacks */
extern const struct sftpprotocol sftp_v3;

/** @brief Pre-initialization protocol callbacks */
extern const struct sftpprotocol sftp_preinit;

/** @brief Selected protocol */
extern const struct sftpprotocol *protocol;

/** @brief What messages this process sends
 *
 * Separately defined as @c request or @c response in the client and server
 * respectively.
 */
extern const char sendtype[];

/** @brief Read-only mode */
extern int readonly;

/** @brief Reverse symlink arguments */
extern int reverse_symlink;

#endif /* GLOBALS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
