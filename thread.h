#ifndef THREAD_H
#define THREAD_H

#if HAVE_PTHREAD_H
# include <pthread.h>
#else
typedef int pthread_mutex_t;
# define pthread_mutex_init(M,A) (0)
# define pthread_mutex_destroy(M) (0)
# define pthread_mutex_lock(M) (0)
# define pthread_mutex_unlock(M) (0)
# define PTHREAD_MUTEX_INITIALIZER (0)
#endif

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
