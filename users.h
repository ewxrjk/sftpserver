#ifndef USERS_H
#define USERS_H

#include <sys/types.h>

struct allocator;

const char *uid2name(struct allocator *a, uid_t uid);
const char *gid2name(struct allocator *a, gid_t gid);

#endif /* USERS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
