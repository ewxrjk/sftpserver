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

/** @file xfns.h @brief Die-on-error utility function interface */

#ifndef XFNS_H
#  define XFNS_H

/** @brief Close @p fd
 * @param fd File descriptor to close
 *
 * Calls sftp_fatal() on error.
 */
void sftp_xclose(int fd);

/** @brief Duplicate @p fd
 * @param fd File descriptor to duplicate
 * @param newfd New file descriptor
 *
 * Calls sftp_fatal() on error.
 */
void sftp_xdup2(int fd, int newfd);

/** @brief Create a pipe
 * @param pfd Where to store endpoint file descriptors
 *
 * Calls sftp_fatal() on error.
 */
void sftp_xpipe(int *pfd);

/** @brief Write to stdout
 * @param fmt Format string as per @c printf()
 * @param ... Arguments
 * @return Number of characters written
 *
 * Calls sftp_fatal() on error.
 */
int sftp_xprintf(const char *fmt, ...) attribute((format(printf, 1, 2)));

#endif /* XFNS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
