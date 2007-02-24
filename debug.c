#include "sftpserver.h"
#include "debug.h"
#include <ctype.h>
#include <stdarg.h>

#ifdef DEBUGPATH
FILE *debugfp;

void hexdump(FILE *fp, const void *ptr, size_t n) {
  const unsigned char *p = ptr;
  size_t i, j;

  for(i = 0; i < n; i += 16) {
    fprintf(debugfp, "%4zx ", i);
    for(j = 0; j < 16; ++j)
      if(i + j < n)
	fprintf(fp, " %02x", p[i + j]);
      else
	fprintf(fp, "   ");
    fputs("  ", fp);
    for(j = 0; j < 16; ++j)
      if(i + j < n)
	fputc(isprint(p[i + j]) ? p[i+j] : '.', fp);
    fputc('\n', fp);
  }
}

void debug_printf(const char *fmt, ...) {
  va_list ap;

  if(debugfp) {
    const int save_errno = errno;
    va_start(ap, fmt);
    vfprintf(debugfp, fmt, ap);
    va_end(ap);
    fputc('\n', debugfp);
    errno = save_errno;
  }
}
#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
