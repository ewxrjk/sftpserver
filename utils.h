#ifndef UTILS_H
#define UTILS_H

int do_fread(void *buffer, size_t size, size_t count, FILE *fp);
/* Error-checking workalike for fread().  Returns 0 on success, non-0 at
 * EOF. */

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *ptr, size_t n);
void *xrecalloc(void *ptr, size_t n, size_t size);
char *xstrdup(const char *s);
/* Error-checking workalikes for malloc() etc.  recalloc() does not
 * 0-fill expansion. */

#endif /* UTILS_H */


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
