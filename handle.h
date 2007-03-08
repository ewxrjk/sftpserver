#ifndef HANDLE_H
#define HANDLE_H

#include <dirent.h>

struct handleid {
  uint32_t id;
  uint32_t tag;
};
/* A handle ID */

void handle_new_file(struct handleid *id, int fd, const char *path, 
                     unsigned flags);
#define HANDLE_TEXT 0x0001
#define HANDLE_APPEND 0x0002
void handle_new_dir(struct handleid *id, DIR *dp, const char *path);
/* Create new file handle */

unsigned handle_flags(const struct handleid *id);

uint32_t handle_get_fd(const struct handleid *id, 
                       int *fd, const char **pathp,
                       unsigned *flagsp);
uint32_t handle_get_dir(const struct handleid *id,
                        DIR **dp, const char **pathp);
uint32_t handle_close(const struct handleid *id);
/* Extract/close a file handle.  Returns 0 on success, an SSH_FX_ code on error
 * or (uint32_t)-1 for an errno error. */

#endif /* HANDLE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
