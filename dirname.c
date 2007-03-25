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

#include "sftpcommon.h"
#include "alloc.h"
#include "debug.h"
#include "utils.h"
#include <assert.h>
#include <string.h>

const char *sftp_dirname(struct allocator *a, const char *path) {
  const char *ls = strrchr(path, '/');
  if(ls) {
    if(ls != path) {
      const size_t len = ls - path;
      char *d;

      assert(len + 1 != 0);
      d = sftp_alloc(a, len + 1);
      memcpy(d, path, len);
      return d;
    } else
      return "/";
  } else
    return ".";
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
