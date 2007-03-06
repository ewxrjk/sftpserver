#ifndef USERS_H
#define USERS_H

#include <sys/types.h>

struct allocator;

char *uid2name(struct allocator *a, uid_t uid);
char *gid2name(struct allocator *a, gid_t gid);
uid_t name2uid(const char *name);
gid_t name2gid(const char *name);

#endif /* USERS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
