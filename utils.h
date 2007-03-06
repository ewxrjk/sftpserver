#ifndef UTILS_H
#define UTILS_H


int do_read(int fd, void *buffer, size_t size);
/* Error-checking workalike for read().  Returns 0 on success, non-0 at
 * EOF. */

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *ptr, size_t n);
void *xrecalloc(void *ptr, size_t n, size_t size);
char *xstrdup(const char *s);
/* Error-checking workalikes for malloc() etc.  recalloc() does not
 * 0-fill expansion. */

char *append(struct allocator *a, char *s, size_t *ns, 
             const char *t);
char *appendn(struct allocator *a, char *s, size_t *ns, 
              const char *t, size_t lt);
/* Append T to S, expanding if need be.  NS tracks total size of S.  LT is the
 * length of T if present. */

char *my_readlink(struct allocator *a, const char *path);
/* Wrapper for readlink.  Sets errno and returns a null pointer on error. */

char *my_realpath(struct allocator *a, const char *path, unsigned flags);
#define RP_READLINK 0x0001              /* follow symlinks */
#define RP_MAY_NOT_EXIST 0x0002         /* nonexistent paths are OK */
/* Return the real path name of PATH.  Sets errno and returns a null pointer on
 * error.
 *
 * If RP_READLINK is set then symlinks are followed.  Otherwise they are
 * not and the transformation is purely lexical.
 *
 * If RP_MAY_NOT_EXIST is set then the path will be converted even if it does
 * not exist or cannot be accessed.  If it is clear but the path does not exist
 * or cannot be accessed then an error _may_ be returned (but this is not
 * guaranteed).
 *
 * Leaving RP_MAY_NOT_EXIST is an optimization for the case where you're later
 * going to do an existence test.
 */

void fatal(const char *msg, ...)
  attribute((noreturn))
  attribute((format(printf,1,2)));
pid_t xfork(void);
void forked(void);

#endif /* UTILS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
