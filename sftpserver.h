#ifndef SFTPSERVER_H
#define SFTPSERVER_H

#include "sftpcommon.h"

#include <sys/types.h>

/* Maximum numbers in a response to SSH_FXP_READDIR */
#ifndef MAXNAMES
# define MAXNAMES 32
#endif

/* Maximum number of concurrent handles supported */
#ifndef MAXHANDLES
# define MAXHANDLES 128
#endif

/* Maximum read size */
#ifndef MAXREAD
# define MAXREAD 1048576
#endif

/* Maximum request size (NOT IMPLEMENTED!) */
#ifndef MAXREQUEST
# define MAXREQUEST 1048576
#endif

/* Default file permissions */
#ifndef DEFAULT_PERMISSIONS
# define DEFAULT_PERMISSIONS 0755
#endif

void send_status(struct sftpjob *job, 
                 uint32_t status,
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

void v456_sendnames(struct sftpjob *job, 
                    int nnames, const struct sftpattr *names);
void v456_sendattrs(struct sftpjob *job,
 		    const struct sftpattr *attrs);
int v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs);
int v456_encode(struct sftpjob *job, char **path);
int v456_decode(struct sftpjob *job, char **path);
void sftp_v34_rename(struct sftpjob *job);
void sftp_v34_open(struct sftpjob *job);
void sftp_v456_lstat(struct sftpjob *job);
void sftp_v456_stat(struct sftpjob *job);
void sftp_v456_fstat(struct sftpjob *job);
void sftp_v345_realpath(struct sftpjob *job);
void sftp_v56_open(struct sftpjob *job);
void sftp_v56_rename(struct sftpjob *job);
void sftp_v6_realpath(struct sftpjob *job);
void sftp_link(struct sftpjob *job);
void sftp_text_seek(struct sftpjob *job);
void generic_open(struct sftpjob *job, const char *path,
                  uint32_t desired_access, uint32_t flags,
                  struct sftpattr *attrs);
void sftp_space_available(struct sftpjob *job);

void send_errno_status(struct sftpjob *job);
/* Call send_status based on errno */

void send_ok(struct sftpjob *job);
/* Send an OK */

#endif /* SFTPSERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
