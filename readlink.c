#include "sftpcommon.h"
#include "utils.h"
#include "alloc.h"
#include "debug.h"
#include <unistd.h>
#include <errno.h>

char *my_readlink(struct allocator *a, const char *path) {
  size_t nresult = 256, oldnresult = 0;
  char *result = 0;
  int n;

  /* readlink(2) has a rather stupid interface */
  while(nresult > 0 && nresult <= 65536) {
    result = allocmore(a, result, oldnresult, nresult);
    n = readlink(path, result, nresult);
    if(n < 0)
      return 0;
    if((unsigned)n < nresult) {
      result[n] = 0;
      return result;
    }
    nresult *= 2;
  }
  /* We should have wasted at most about 128Kbyte if we get here,
   * perhaps less due to use of allocmore().  If you have symlinks
   * with targets bigger than 64K then (i) you'll need to update the
   * bound above (ii) what color is the sky on your planet? */
  errno = E2BIG;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
