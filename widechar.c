#include "sftpcommon.h"
#include "utils.h"
#include "widechar.h"
#include <string.h>

wchar_t *convertm2w(const char *s) {
  wchar_t *ws;
  size_t len;
  mbstate_t ps;
  
  memset(&ps, 0, sizeof ps);
  len = mbsrtowcs(0, &s, 0, &ps);
  if(len == (size_t)-1)
    return 0;
  ws = xcalloc(len + 1, sizeof *ws);
  memset(&ps, 0, sizeof ps);
  mbsrtowcs(ws, &s, len, &ps);
  return ws;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
