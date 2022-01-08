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

#include "sftpserver.h"
#include "utils.h"
#include "xfns.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

void sftp_xclose(int fd) {
  if(close(fd) < 0)
    sftp_fatal("error calling close: %s", strerror(errno));
}

void sftp_xdup2(int fd, int newfd) {
  if(dup2(fd, newfd) < 0)
    sftp_fatal("error calling dup2: %s", strerror(errno));
}

void sftp_xpipe(int *pfd) {
  if(pipe(pfd) < 0)
    sftp_fatal("error calling pipe: %s", strerror(errno));
}

int sftp_xprintf(const char *fmt, ...) {
  va_list ap;
  int rc;

  va_start(ap, fmt);
  rc = vfprintf(stdout, fmt, ap);
  va_end(ap);
  if(rc < 0)
    sftp_fatal("error writing to stdout: %s", strerror(errno));
  return rc;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
