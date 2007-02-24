#include "sftpserver.h"
#include "utils.h"
#include "xfns.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

void xclose(int fd) {
  if(close(fd) < 0) fatal("error calling close: %s", strerror(errno));
}

void xdup2(int fd, int newfd) {
  if(dup2(fd, newfd) < 0) fatal("error calling dup2: %s", strerror(errno));
}

void xpipe(int *pfd) {
  if(pipe(pfd) < 0) fatal("error calling pipe: %s", strerror(errno));
}

FILE *xfdopen(int fd, const char *mode) {
  FILE *fp;

  if(!(fp = fdopen(fd, mode)))
    fatal("error calling fdopen: %s", strerror(errno));
  return fp;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
