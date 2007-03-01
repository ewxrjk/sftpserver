#ifndef DEBUG_H
#define DEBUG_H

/* Debug support */

extern int debugging;
extern const char *debugpath;

void hexdump(const void *ptr, size_t n);
void debug_printf(const char *fmt, ...) attribute((format(printf,1,2)));
#define D(x) do {                               \
  if(debugging)                                 \
    debug_printf x;				\
} while(0)

#endif /* DEBUG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
