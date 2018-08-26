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

/** @file stat.h @brief SFTP attribute support interface */

#ifndef STAT_H
#  define STAT_H

/** @brief Produce a human-readable representation of file attributes
 * @param a Allocator
 * @param attrs Attributes to format
 * @param thisyear This year-1900
 * @param flags Flags
 * @return Formatted attributes
 *
 * Flag values:
 * - @ref FORMAT_PREFER_NUMERIC_UID
 * - @ref FORMAT_PREFER_LOCALTIME
 * - @ref FORMAT_ATTRS
 */
const char *sftp_format_attr(struct allocator *a, const struct sftpattr *attrs,
                             int thisyear, unsigned long flags);
/** @brief User numeric instead of named user and group IDs
 *
 * See sftp_format_attr().
 */
#  define FORMAT_PREFER_NUMERIC_UID 0x00000001

/** @brief Use local time instead of UTC
 *
 * See sftp_format_attr().
 */
#  define FORMAT_PREFER_LOCALTIME 0x00000002

/** @brief Include attribute bits
 *
 * See sftp_format_attr().
 */
#  define FORMAT_ATTRS 0x00000004

/** @brief Fill in missing owner/group attributes
 * @param a Allocator
 * @param attrs Attributes
 * @return 0 on success, status code on error
 *
 * If exactly one of @ref SSH_FILEXFER_ATTR_UIDGID and @ref
 * SSH_FILEXFER_ATTR_OWNERGROUP is present then attempt to fill in the other.
 */
uint32_t sftp_normalize_ownergroup(struct allocator *a, struct sftpattr *attrs);

/** @brief Set the attributes on a path name
 * @param a Allocator
 * @param path Path to modify
 * @param attrs Attributes to set
 * @param whyp Where to store error string, or a null pointer
 * @return 0 on success or an error code on failure
 */
uint32_t sftp_set_status(struct allocator *a, const char *path,
                         const struct sftpattr *attrs, const char **whyp);

/** @brief Set the attributes on an open file
 * @param a Allocator
 * @param fd File descriptor to modify
 * @param attrs Attributes to set
 * @param whyp Where to store error string, or a null pointer
 * @return 0 on success or an error code on failure
 */
uint32_t sftp_set_fstatus(struct allocator *a, int fd,
                          const struct sftpattr *attrs, const char **whyp);

/** @brief Convert @c stat() output to SFTP attributes
 * @param a Allocator
 * @param sb Result of calling @c stat() or similar
 * @param attrs Where to store SFTP attributes
 * @param flags Requested attributes
 * @param path Path name or a null pointer
 *
 * The only meaningful bit in @p flags currently is @ref
 * SSH_FILEXFER_ATTR_OWNERGROUP, which causes the owner and group names to be
 * filled in.
 *
 * @todo @ref SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE is not implemented.
 *
 * @todo @ref SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE is not implemented.
 */
void sftp_stat_to_attrs(struct allocator *a, const struct stat *sb,
                        struct sftpattr *attrs, uint32_t flags,
                        const char *path);

#endif /* STAT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
