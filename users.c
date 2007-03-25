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
#include "users.h"
#include "alloc.h"
#include "thread.h"
#include "utils.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* We don't rely on the C library doing the right thing */
static pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;

char *sftp_uid2name(struct allocator *a, uid_t uid) {
  char *s;
  const struct passwd *pw;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((pw = getpwuid(uid)))
    s = strcpy(sftp_alloc(a, strlen(pw->pw_name) + 1), pw->pw_name);
  else
    s = 0;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

char *sftp_gid2name(struct allocator *a, gid_t gid) {
  char *s;
  const struct group *gr;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((gr = getgrgid(gid)))
    s = strcpy(sftp_alloc(a, strlen(gr->gr_name) + 1), gr->gr_name);
  else
    s = 0;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

uid_t sftp_name2uid(const char *name) {
  const struct passwd *pw;
  uid_t uid;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((pw = getpwnam(name)))
    uid = pw->pw_uid;
  else
    uid = -1;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return uid;
}

gid_t sftp_name2gid(const char *name) {
  const struct group *gr;
  gid_t gid;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((gr = getgrnam(name)))
    gid = gr->gr_gid;
  else
    gid = -1;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return gid;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
