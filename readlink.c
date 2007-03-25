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
#include "utils.h"
#include "alloc.h"
#include "debug.h"
#include <unistd.h>
#include <errno.h>

char *sftp_do_readlink(struct allocator *a, const char *path) {
  size_t nresult = 32, oldnresult = 0;
  char *result = 0;
  int n;

  /* readlink(2) has a rather stupid interface */
  while(nresult > 0 && nresult <= 65536) {
    result = sftp_alloc_more(a, result, oldnresult, nresult);
    n = readlink(path, result, nresult);
    if(n < 0)
      return 0;
    if((unsigned)n < nresult) {
      result[n] = 0;
      return result;
    }
    oldnresult = nresult;
    nresult *= 2;
  }
  /* We should have wasted at most about 128Kbyte if we get here,
   * perhaps less due to use of sftp_alloc_more().  If you have symlinks
   * with targets bigger than 64K then (i) you'll need to update the
   * bound above (ii) what color is the sky on your planet? */
  errno = E2BIG;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
