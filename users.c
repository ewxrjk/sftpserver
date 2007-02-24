#include "sftpserver.h"
#include "users.h"
#include "alloc.h"
#include <pwd.h>
#include <grp.h>

#if HAVE_PTHREAD_H
/* We don't rely on the C library doing the right thing */
static pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

const char *uid2name(struct allocator *a, uid_t uid) {
  char *s;
  const struct passwd *pw;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((pw = getpwuid(uid)))
    s = strcpy(alloc(a, strlen(pw->pw_name) + 1), pw->pw_name);
  else
    sprintf((s = alloc(a, 32)), "%jd", (intmax_t)uid);
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

const char *gid2name(struct allocator *a, gid_t gid) {
  char *s;
  const struct group *gr;

  ferrcheck(pthread_mutex_lock(&user_lock));
  if((gr = getgrgid(gid)))
    s = strcpy(alloc(a, strlen(gr->gr_name) + 1), gr->gr_name);
  else
    sprintf((s = alloc(a, 32)), "%jd", (intmax_t)gid);
  ferrcheck(pthread_mutex_unlock(&user_lock));
  return s;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
