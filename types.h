#ifndef TYPES_H
#define TYPES_H

#include <sys/stat.h>

struct sftptime {
  int64_t seconds;
  uint32_t nanoseconds;
};

struct sftpattr {
  uint32_t valid;                       /* flags per v6 */
  uint8_t type;
  uint64_t size;
  uint64_t allocation_size;             /* v6+ */
  uid_t uid, gid;                       /* v3 */ 
  const char *owner, *group;            /* v4+ */
  uint32_t permissions;
  struct sftptime atime;
  struct sftptime createtime;           /* v4+ */
  struct sftptime mtime;
  struct sftptime ctime;                /* v6+ */
  char *acl;                            /* v5+ */
  uint32_t attrib_bits;                 /* v5+ */
  /* all v6+: */
  uint32_t attrib_bits_valid;
  uint8_t text_hint;
  char *mime_type;
  uint32_t link_count;
  char *untranslated_name;
  /* We stuff these in here too so we can conveniently use sftpattrs for
   * name lists */
  char *name;
  char *longname;
};
/* SFTP-style file attributes */

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
                    int nnames, const struct sftpattr *names);
  void (*sendattrs)(struct sftpjob *job, const struct sftpattr *filestat);
  int (*parseattrs)(struct sftpjob *job, struct sftpattr *filestat);
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
