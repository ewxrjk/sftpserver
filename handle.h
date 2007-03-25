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

#ifndef HANDLE_H
#define HANDLE_H

#include <dirent.h>

struct handleid {
  uint32_t id;
  uint32_t tag;
};
/* A handle ID */

void sftp_handle_new_file(struct handleid *id, int fd, const char *path, 
                          unsigned flags);
#define HANDLE_TEXT 0x0001
#define HANDLE_APPEND 0x0002
void sftp_handle_new_dir(struct handleid *id, DIR *dp, const char *path);
/* Create new file handle */

unsigned sftp_handle_flags(const struct handleid *id);

uint32_t sftp_handle_get_fd(const struct handleid *id, 
                            int *fd,
                            unsigned *flagsp);
uint32_t sftp_handle_get_dir(const struct handleid *id,
                             DIR **dp, const char **pathp);
uint32_t sftp_handle_close(const struct handleid *id);
/* Extract/close a file handle.  Returns 0 on success, an SSH_FX_ code on error
 * or (uint32_t)-1 for an errno error. */

#endif /* HANDLE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
