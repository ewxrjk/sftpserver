#include "sftpserver.h"
#include "utils.h"
#include "xfns.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

void xclose(int fd) {
  if(close(fd) < 0) fatal("error calling close: %s", strerror(errno));
}

void xdup2(int fd, int newfd) {
  if(dup2(fd, newfd) < 0) fatal("error calling dup2: %s", strerror(errno));
}

void xpipe(int *pfd) {
  if(pipe(pfd) < 0) fatal("error calling pipe: %s", strerror(errno));
}

int xprintf(const char *fmt, ...) {
  va_list ap;
  int rc;

  va_start(ap, fmt);
  rc = vfprintf(stdout, fmt, ap);
  va_end(ap);
  if(rc < 0) fatal("error writing to stdout: %s", strerror(errno));
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
