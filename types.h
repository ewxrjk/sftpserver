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
  int (*parseattrs)(struct sftpjob *job, struct stat *filestat,
		    unsigned long *bits);
  void (*encode)(struct sftpjob *job,
                 char **path);          /* Convert from local to wire */
  int (*decode)(struct sftpjob *job,
                char **path);           /* Convert from wire to local */
  
};
/* An SFTP protocol version */

/* Bits set in a parseattrs result */
#define ATTR_SIZE		0x00000001
#define ATTR_UID		0x00000002
#define ATTR_GID		0x00000004
#define ATTR_PERMISSIONS	0x00000008
#define ATTR_MTIME		0x00000010
#define ATTR_ATIME		0x00000020
#define ATTR_CTIME		0x00000040

#endif /* TYPES_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
