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

/** @file handle.h @brief File handle interface */

#ifndef HANDLE_H
#define HANDLE_H

#include <dirent.h>

/** @brief Definition of a handle */
struct handleid {
  /** @brief Handle ID
   *
   * This is an index into the @ref handles array.  At any given moment no two
   * valid handles have the same ID, but handles that do not overlap in
   * lifetime will often have matching IDs.
   */
  uint32_t id;

  /** @brief Handle tag
   *
   * This is a sequence number, used to distinguish handles with the same ID.
   */
  uint32_t tag;
};

/** @brief Create a new file handle
 * @param id Where to store new handle
 * @param fd File descriptor to attach to handle
 * @param path Path name to attach to handle (will be copied)
 * @param flags Handle flags
 *
 * Valid handle flags are:
 * - @ref HANDLE_TEXT
 * - @ref HANDLE_APPEND
 */
void sftp_handle_new_file(struct handleid *id, int fd, const char *path, 
                          unsigned flags);

/** @brief Handle flag for text files
 *
 * See sftp_handle_new_file().
 */
#define HANDLE_TEXT 0x0001

/** @brief Handle flag for append-mode files
 *
 * See sftp_handle_new_file().
 */
#define HANDLE_APPEND 0x0002

/** @brief Create a new directory handle
 * @param id Where to store new handle
 * @param dp Directory stream to attach to handle
 * @param path Path name to attach to handle (will be copied)
 */
void sftp_handle_new_dir(struct handleid *id, DIR *dp, const char *path);

/** @brief Retrieve the flags for handle @p id
 * @param id Handle
 * @return Flag values
 *
 * If the handle is not valid, 0 is returned.
 */
unsigned sftp_handle_flags(const struct handleid *id);

/** @brief Retrieve the file descriptor attached to handle @p id
 * @param id Handle
 * @param fd Where to store file descriptor
 * @param flagsp Where to store flags, or a null pointer
 * @return 0 on success, @ref SSH_FX_INVALID_HANDLE on error
 */
uint32_t sftp_handle_get_fd(const struct handleid *id, 
                            int *fd,
                            unsigned *flagsp);

/** @brief Retrieve the directory stream attached to handle @p id
 * @param id Handle
 * @param dp Where to store directory stream
 * @param pathp Where to store path, or a null pointer
 * @return 0 on success, @ref SSH_FX_INVALID_HANDLE on error
 *
 * If @p pathp is not a null pointer then the value assigned to @c *pathp
 * points to the handle's copy of the path name.  It will not outlive the
 * handle.
 */
uint32_t sftp_handle_get_dir(const struct handleid *id,
                             DIR **dp, const char **pathp);

/** @brief Destroy a handle
 * @param id Handle to close
 * @return 0 on success, SFTP error code or HANDLER_ERRNO on error
 *
 * The attached file descriptor or director stream is closed.
 */
uint32_t sftp_handle_close(const struct handleid *id);

#endif /* HANDLE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
