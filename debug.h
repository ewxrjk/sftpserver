#ifndef DEBUG_H
#define DEBUG_H

/* Debug support */

#ifdef DEBUGPATH
#define DEBUG 1
void debug_init(void);
void hexdump(const void *ptr, size_t n);
void debug_printf(const char *fmt, ...) attribute((format(printf,1,2)));
#define D(x) do {                               \
  debug_printf x;				\
} while(0)
#else
#define DEBUG 0
#define D(x) /* nothing */
#define hexdump(PTR,N) /* nothing */
#endif

#endif /* DEBUG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
