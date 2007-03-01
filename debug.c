#include "sftpserver.h"

#include "debug.h"
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

static FILE *debugfp;
const char *debugpath;
int debugging;

static void opendebug(void) {
  assert(debugging);
  if(!debugfp) {
    if(debugpath)
      debugfp = fopen(debugpath, "w");
    if(!debugfp)
      debugfp = stderr;
  }
}

void hexdump(const void *ptr, size_t n) {
  const unsigned char *p = ptr;
  size_t i, j;

  opendebug();
  for(i = 0; i < n; i += 16) {
    fprintf(debugfp, "%4zx ", i);
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
