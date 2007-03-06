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

char *uid2name(struct allocator *a, uid_t uid) {
  char *s;
  const struct passwd *pw;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((pw = getpwuid(uid)))
    s = strcpy(alloc(a, strlen(pw->pw_name) + 1), pw->pw_name);
  else
    s = 0;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

char *gid2name(struct allocator *a, gid_t gid) {
  char *s;
  const struct group *gr;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((gr = getgrgid(gid)))
    s = strcpy(alloc(a, strlen(gr->gr_name) + 1), gr->gr_name);
  else
    s = 0;
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

uid_t name2uid(const char *name) {
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

gid_t name2gid(const char *name) {
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
