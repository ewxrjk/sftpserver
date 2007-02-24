#ifndef XFNS_H
#define XFNS_H

#include <stdio.h>

void xclose(int fd);
void xdup2(int fd, int newfd);
void xpipe(int *pfd);
FILE *xfdopen(int fd, const char *mode);

#endif /* XFNS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
