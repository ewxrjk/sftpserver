#ifndef XFNS_H
#define XFNS_H

void xclose(int fd);
void xdup2(int fd, int newfd);
void xpipe(int *pfd);
int xprintf(const char *fmt, ...) attribute((format(printf,1,2)));

#endif /* XFNS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
