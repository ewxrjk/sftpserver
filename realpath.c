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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

static char *process_path(struct allocator *a, char *result, size_t *nresultp,
			  const char *path, unsigned flags);

char *my_realpath(struct allocator *a, const char *path, unsigned flags) {
  char *cwd, *abspath, *result = 0;
  size_t nresult = 0;

  /* Default is current directory */
  if(path[0] == 0)
    path = ".";

  /* Convert relative paths to absolute paths */
  if(path[0] != '/') {
    cwd = xmalloc(PATH_MAX);
    if(!getcwd(cwd, PATH_MAX)) {
      free(cwd);
      return 0;
    }
    assert(cwd[0] == '/');
    abspath = alloc(a, strlen(cwd) + strlen(path) + 2);
    strcpy(abspath, cwd);
    strcat(abspath, "/");
    strcat(abspath, path);
    path = abspath;
    free(cwd);
  }

  /* The result always starts with a / */
  result = append(a, result, &nresult, "/");

  /* All the work happens below */
  return process_path(a, result, &nresult, path, flags);
}

static char *process_path(struct allocator *a, char *result, size_t *nresultp,
			  const char *path, unsigned flags) {
  while(*path) {
    if(*path == '/')
      ++path;
    else {
      /* Find the next path element */
      const size_t elementlen = strcspn(path, "/");

      if(elementlen == 1 && path[0] == '.')
        /* Stay where we are */
        ;
      else if(elementlen == 2 && path[0] == '.' && path[1] == '.') {
        /* Go up one level */
        char *const ls = strrchr(result, '/');
        assert(ls != 0);
        if(ls != result)
          *ls = 0;
        else
          strcpy(result, "/");          /* /.. = / */
      } else {
        const size_t oldresultlen = strlen(result);
        /* Append the new path element */
        if(result[1])
          result = append(a, result, nresultp, "/");
        result = appendn(a, result, nresultp, path, elementlen);
        /* If we're following symlinks, see if the path so far points to a
         * link */
        if(flags & RP_READLINK) {
          const char *const target = my_readlink(a, result);
        
          if(target) {
            if(target[0] == '/')
              /* Absolute symlink, go back to the root */
              strcpy(result, "/");
            else
              /* Relative symlink, lose the last path element */
              result[oldresultlen] = 0;
            /* Process all! the elements of the link target */
            result = process_path(a, result, nresultp, target, flags);
          } else if(errno != EINVAL && !(flags & RP_MAY_NOT_EXIST))
            /* EINVAL means it wasn't a link.  Anything else means something
             * went wrong while resolving it.  If we've not been asked to
             * handle nonexistent paths we save the remaining effort and return
             * an error straight away. */
            return 0;
        }
      }
      path += elementlen;
    }
  }
  return result;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
