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
int v3_parseattrs(struct sftpjob *job, struct stat *filestat,
                  unsigned long *bits);
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

#endif /* SFTPSERVER_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
