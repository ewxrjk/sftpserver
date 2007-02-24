#ifndef UTILS_H
#define UTILS_H

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *ptr, size_t n);
void *xrecalloc(void *ptr, size_t n, size_t size);
char *xstrdup(const char *s);
/* Error-checking workalikes for malloc() etc.  recalloc() does not
 * 0-fill expansion. */

void fatal(const char *msg, ...)
  attribute((noreturn))
  attribute((format(printf,1,2)));
pid_t xfork(void);

#endif /* UTILS_H */


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
