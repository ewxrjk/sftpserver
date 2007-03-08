#include "sftpserver.h"

#include "debug.h"
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>

static FILE *debugfp;
const char *debugpath;
int debugging;

static void opendebug(void) {
  assert(debugging);
  if(!debugfp) {
    if(debugpath) {
      int fd;

      if((fd = open(debugpath, O_WRONLY|O_CREAT|O_TRUNC, 0600)) >= 0)
        debugfp = fdopen(fd, "w");
      else
        fprintf(stderr, "%s: %s\n", debugpath, strerror(errno));
    }
    if(!debugfp)
      debugfp = stderr;
  }
}

void hexdump(const void *ptr, size_t n) {
  const unsigned char *p = ptr;
  size_t i, j;

  opendebug();
  for(i = 0; i < n; i += 16) {
    fprintf(debugfp, "%4lx ", (unsigned long)i);
    for(j = 0; j < 16; ++j)
      if(i + j < n)
	fprintf(debugfp, " %02x", p[i + j]);
      else
	fprintf(debugfp, "   ");
    fputs("  ", debugfp);
    for(j = 0; j < 16; ++j)
      if(i + j < n)
	fputc(isprint(p[i + j]) ? p[i+j] : '.', debugfp);
    fputc('\n', debugfp);
    fflush(debugfp);
  }
}

void debug_printf(const char *fmt, ...) {
  va_list ap;
  const int save_errno = errno;

  opendebug();
  va_start(ap, fmt);
  vfprintf(debugfp, fmt, ap);
  va_end(ap);
  fputc('\n', debugfp);
  fflush(debugfp);
  errno = save_errno;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
