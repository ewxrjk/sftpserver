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

#ifndef STAT_H
#define STAT_H

const char *sftp_format_attr(struct allocator *a,
                             const struct sftpattr *attrs, int thisyear,
                             unsigned long flags);
#define FORMAT_PREFER_NUMERIC_UID 0x00000001
#define FORMAT_PREFER_LOCALTIME 0x00000002
#define FORMAT_ATTRS 0x00000004
/* Prefer numeric UID instead of names */

uint32_t sftp_normalize_ownergroup(struct allocator *a,
                                   struct sftpattr *attrs);

uint32_t sftp_set_status(struct allocator *a,
                         const char *path,
                         const struct sftpattr *attrs,
                         const char **whyp);
uint32_t sftp_set_fstatus(struct allocator *a,
                          int fd,
                          const struct sftpattr *attrs,
                          const char **whyp);
void sftp_stat_to_attrs(struct allocator *a,
                        const struct stat *sb, struct sftpattr *attrs,
                        uint32_t flags, const char *path);

#endif /* STAT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
