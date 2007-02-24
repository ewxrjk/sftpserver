#ifndef TYPES_H
#define TYPES_H

#include <sys/stat.h>

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

#endif /* TYPES_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
