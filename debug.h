#ifndef DEBUG_H
#define DEBUG_H

/* Debug support */

#ifdef DEBUGPATH
extern FILE *debugfp;

void hexdump(FILE *fp, const void *ptr, size_t n);
void debug_printf(const char *fmt, ...);
#define D(x) do {                               \
  if(debugfp)                                   \
    debug_printf x;                             \
} while(0)
#else
#define debugfp 0
#define D(x) /* nothing */
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
