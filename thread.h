#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

/* Error-checking for pthreads functions */
#define ferrcheck(E) do {                                       \
  const int frc = (E);                                          \
  if(frc) {                                                     \
    fatal("%s: %s\n", #E, strerror(frc));                       \
    exit(1);                                                    \
  }                                                             \
} while(0)

#endif /* THREAD_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
