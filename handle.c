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
#include "debug.h"
#include "utils.h"
#include "sftp.h"
#include "handle.h"
#include "thread.h"
#include "types.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

struct handle {
  int type;                             /* SSH_FXP_OPEN/OPENDIR */
  uint32_t tag;                         /* unique tag */
  union {
    int fd;                             /* file descriptor for a file */
    DIR *dir;                           /* directory handle */
  } u;
  char *path;                           /* name of file or directory */
  unsigned flags;                       /* flags */
};
/* A file handle */

static struct handle *handles;
static size_t nhandles;
static uint32_t sequence;
static pthread_mutex_t sftp_handle_lock = PTHREAD_MUTEX_INITIALIZER;

static void find_free_handle(struct handleid *id, int type) {
  size_t n;

  for(n = 0; n < nhandles && handles[n].tag; ++n)
    ;
  if(n == nhandles && nhandles < MAXHANDLES) {
    /* need more space */
    nhandles = (nhandles ? 2 * nhandles : 16);
    assert(nhandles != 0);
    handles = xrecalloc(handles, nhandles, sizeof (*handles));
    memset(handles + n, 0, (nhandles - n) * sizeof (*handles));
  }
  while(!sequence)
    ++sequence;                         /* never have a tag of 0 */
  handles[n].tag = sequence++;
  handles[n].type = type;
  id->id = n;
  id->tag = handles[n].tag;
}

void sftp_handle_new_file(struct handleid *id, 
                     int fd, const char *path, unsigned flags) {
  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  find_free_handle(id, SSH_FXP_OPEN);
  handles[id->id].u.fd = fd;
  handles[id->id].path = xstrdup(path);
  handles[id->id].flags = flags;
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
}

void sftp_handle_new_dir(struct handleid *id, DIR *dp, const char *path) {
  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  find_free_handle(id, SSH_FXP_OPENDIR);
  handles[id->id].u.dir = dp;
  handles[id->id].path = xstrdup(path);
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
}

uint32_t sftp_handle_get_fd(const struct handleid *id,
                       int *fd, unsigned *flagsp) {
  uint32_t rc;

  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  if(id->id < nhandles
     && id->tag == handles[id->id].tag
     && handles[id->id].type == SSH_FXP_OPEN) {
    *fd = handles[id->id].u.fd;
    if(flagsp)
      *flagsp = handles[id->id].flags;
    rc = 0;
  } else
    rc = SSH_FX_INVALID_HANDLE;
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
  return rc;
}

uint32_t sftp_handle_get_dir(const struct handleid *id,
                        DIR **dp, const char **pathp) {
  uint32_t rc;

  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  if(id->id < nhandles
     && id->tag == handles[id->id].tag
     && handles[id->id].type == SSH_FXP_OPENDIR) {
    *dp = handles[id->id].u.dir;
    if(pathp)
      *pathp = handles[id->id].path;
    rc = 0;
  } else
    rc = SSH_FX_INVALID_HANDLE;
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
  return rc;
}

uint32_t sftp_handle_close(const struct handleid *id) {
  uint32_t rc;

  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  if(id->id < nhandles
     && id->tag == handles[id->id].tag) {
    handles[id->id].tag = 0;            /* free up */
    switch(handles[id->id].type) {
    case SSH_FXP_OPEN:
      if(close(handles[id->id].u.fd) < 0)
        rc = HANDLER_ERRNO;
      else
        rc = 0;
      break;
    case SSH_FXP_OPENDIR:
      if(closedir(handles[id->id].u.dir) < 0)
        rc = HANDLER_ERRNO;
      else
        rc = 0;
      break;
    default:
      rc = SSH_FX_INVALID_HANDLE;
    }
    free(handles[id->id].path);
  }
  else
    rc = SSH_FX_INVALID_HANDLE;
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
  return rc;
}

unsigned sftp_handle_flags(const struct handleid *id) {
  unsigned rc;

  ferrcheck(pthread_mutex_lock(&sftp_handle_lock));
  if(id->id < nhandles
     && id->tag == handles[id->id].tag)
    rc = handles[id->id].flags;
  else
    rc = 0;
  ferrcheck(pthread_mutex_unlock(&sftp_handle_lock));
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
