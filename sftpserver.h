#ifndef SFTPSERVER_H
#define SFTPSERVER_H

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <inttypes.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "sftp.h"
#include "utils.h"

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

/* Maximum numbers in a response to SSH_FXP_READDIR */
#ifndef MAXNAMES
# define MAXNAMES 32
#endif

/* Error-checking for pthreads functions */
#define ferrcheck(E) do {                                       \
  const int frc = (E);                                          \
  if(frc) {                                                     \
    fprintf(stderr, "FATAL: %s: %s\n", #E, strerror(frc));      \
    exit(1);                                                    \
  }                                                             \
} while(0)

struct queue;
struct allocator;

struct namedata {
  char *path;
  struct stat filestat;
  int dummy;                            /* true for dummy attributes */
};
/* data about a filename */

struct worker {
  size_t bufsize, bufused;
  uint8_t *buffer;
};
/* Thread-specific data */

struct sftpjob {
  size_t len;
  unsigned char *data;                  /* whole job */
  size_t left;
  const unsigned char *ptr;             /* unparsed portion of job */
  struct allocator *a;                  /* allocator */
  uint32_t id;                          /* ID or 0 */
  struct worker *worker;                /* worker processing this job */
};
/* An SFTP job. */

struct sftpcmd {
  uint8_t type;                         /* message type */
  void (*handler)(struct sftpjob *job);
};

struct sftpprotocol {
  int ncommands;
  const struct sftpcmd *commands;       /* sorted by type */
  void (*status)(struct sftpjob *job, 
                 uint32_t status,
                 const char *msg);      /* Send an SSH_FXP_STATUS */
  void (*sendnames)(struct sftpjob *job, 
                    int nnames, const struct namedata *names);
  void (*sendattrs)(struct sftpjob *job, const struct stat *filestat,
                    int dummy);
  void (*encode)(struct sftpjob *job,
                 char **path);          /* Convert from local to wire */
  int (*decode)(struct sftpjob *job,
                char **path);           /* Convert from wire to local */
  
};
/* An SFTP protocol version */

struct handleid {
  uint32_t id;
  uint32_t tag;
};
/* A handle ID */

void generic_status(struct sftpjob *job,
                    uint32_t status,
                    uint32_t original_status,
                    const char *msg);
/* Send an SSH_FXP_STATUS */

void sftp_already_init(struct sftpjob *job);
void sftp_remove(struct sftpjob *job);
void sftp_rmdir(struct sftpjob *job);
void sftp_symlink(struct sftpjob *job);
void sftp_readlink(struct sftpjob *job);
void sftp_close(struct sftpjob *job);
void sftp_read(struct sftpjob *job);
void sftp_write(struct sftpjob *job);
void sftp_setstat(struct sftpjob *job);
void sftp_fsetstat(struct sftpjob *job);
void sftp_opendir(struct sftpjob *job);
void sftp_readdir(struct sftpjob *job);
void sftp_mkdir(struct sftpjob *job);
void sftp_extended(struct sftpjob *job);

void v3_status(struct sftpjob *job,
               uint32_t status,
               const char *msg);
void v3_sendnames(struct sftpjob *job, 
                  int nnames, const struct namedata *names);
void v3_sendattrs(struct sftpjob *job, const struct stat *filestat,
                  int dummy);
void v3_encode(struct sftpjob *job, char **path);
int v3_decode(struct sftpjob *job, char **path);
void sftp_v3_rename(struct sftpjob *job);
void sftp_v3_open(struct sftpjob *job);
void sftp_v3_lstat(struct sftpjob *job);
void sftp_v3_stat(struct sftpjob *job);
void sftp_v3_fstat(struct sftpjob *job);
void sftp_v3_realpath(struct sftpjob *job);

void send_errno_status(struct sftpjob *job);
/* Call protocol->status based on errno */

void send_ok(struct sftpjob *job);
/* Send an OK */

int parse_uint32(struct sftpjob *job, uint32_t *ur);
int parse_string(struct sftpjob *job, char **strp, size_t *lenp);
int parse_path(struct sftpjob *job, char **strp);
int parse_handle(struct sftpjob *job, struct handleid *id);
/* Parse various values out of the remaining data of a job.  Return 0 on
 * success, non-0 on error. */

#define pcheck(E) do {                                          \
  if((E)) {                                                     \
    if(debugfp)                                                 \
      fprintf(debugfp, "%s:%d: %s\n", __FILE__, __LINE__, #E);  \
    protocol->status(job, SSH_FX_BAD_MESSAGE, 0);               \
    return;                                                     \
  }                                                             \
} while(0)
/* error-checking wrapper for parse_ functions */

void handle_new_file(struct handleid *id, int fd, const char *path);
void handle_new_dir(struct handleid *id, DIR *dp, const char *path);
/* Create new file handle */

uint32_t handle_get_fd(const struct handleid *id, 
                       int *fd, const char **pathp);
uint32_t handle_get_dir(const struct handleid *id,
                        DIR **dp, const char **pathp);
uint32_t handle_close(const struct handleid *id);
/* Extract/close a file handle.  Returns 0 on success, an SSH_FX_ code on error
 * or (uint32_t)-1 for an errno error. */

void send_begin(struct sftpjob *job);
void send_end(struct sftpjob *job);
void send_uint8(struct sftpjob *job, int n);
void send_uint32(struct sftpjob *job, uint32_t u);
void send_uint64(struct sftpjob *job, uint64_t u);
void send_bytes(struct sftpjob *job, const void *bytes, size_t n);
void send_handle(struct sftpjob *job, const struct handleid *id);
#define send_string(JOB, S) do {                \
  const char *s_ = (S);                         \
  send_bytes(JOB, s_, strlen(s_));              \
} while(0)
void send_path(struct sftpjob *job, const char *path);

#if HAVE_PTHREAD_H
extern struct queue *workqueue;
/* Queue to which jobs are sent */
#endif

extern const struct sftpprotocol sftpv3, sftppreinit;
extern const struct sftpprotocol *protocol;

#endif /* SFTPSERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
