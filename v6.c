#include "sftpserver.h"
#include "types.h"
#include "sftp.h"
#include "alloc.h"
#include "stat.h"
#include "parse.h"
#include "send.h"
#include "debug.h"
#include "utils.h"
#include "globals.h"
#include <unistd.h>
#include <string.h>

void sftp_v6_realpath(struct sftpjob *job) {
  char *path, *compose;
  struct sftpattr attr;
  uint8_t control_byte = SSH_FXP_REALPATH_NO_CHECK;
  unsigned rpflags = 0;
  struct stat sb;
  struct sftpattr attrs;

  pcheck(parse_path(job, &path));
  if(job->left) {
    pcheck(parse_uint8(job, &control_byte));
    while(job->left) {
      pcheck(parse_path(job, &compose));
      if(compose[0] == '/')
        path = compose;
      else {
        char *newpath = alloc(job->a, strlen(path) + strlen(compose) + 2);

        strcpy(newpath, path);
        strcat(newpath, "/");
        strcat(newpath, compose);
        path = newpath;
      }
    }
  }
  D(("sftp_v6_realpath %s %#x", path, control_byte));
  switch(control_byte) {
  case SSH_FXP_REALPATH_NO_CHECK:
    /* Don't follow links and don't fail if the path doesn't exist */
    rpflags = RP_MAY_NOT_EXIST;
    break;
  case SSH_FXP_REALPATH_STAT_IF:
    /* Follow links but don't fail if the path doesn't exist */
    rpflags = RP_READLINK|RP_MAY_NOT_EXIST;
    break;
  case SSH_FXP_REALPATH_STAT_ALWAYS:
    /* Follow links and fail if the path doesn't exist */
    rpflags = RP_READLINK;
    break;
  default:
    send_status(job, SSH_FX_BAD_MESSAGE,
                "unknown SSH_FXP_REALPATH control-byte value");
    return;
  }
  memset(&attr, 0, sizeof attr);
  if(!(attr.name = my_realpath(job->a, path, RP_READLINK|RP_MAY_NOT_EXIST))) {
    send_errno_status(job);
    return;
  }
  D(("...real path is %s", attr.name));
  switch(control_byte) {
  case SSH_FXP_REALPATH_NO_CHECK:
    /* Don't stat, send dummy attributes */
    break;
  case SSH_FXP_REALPATH_STAT_IF:
    /* stat but accept failure */
    if(lstat(path, &sb) >= 0)
      stat_to_attrs(job->a, &sb, &attrs, 0xFFFFFFFF, 0);
    break;
  case SSH_FXP_REALPATH_STAT_ALWAYS:
    /* stat and error on failure */
    if(stat(path, &sb) >= 0)
      stat_to_attrs(job->a, &sb, &attrs, 0xFFFFFFFF, 0);
    else {
      send_errno_status(job);
      return;
    }
    break;
  }
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_NAME);
  send_uint32(job->worker, job->id);
  protocol->sendnames(job, 1, &attr);
  send_end(job->worker);
}

void sftp_link(struct sftpjob *job) {
  char *oldpath, *newpath;
  uint8_t symbolic;

  pcheck(parse_path(job, &newpath));    /* aka new-link-path */
  pcheck(parse_path(job, &oldpath));    /* aka existing-path/target-paths */
  pcheck(parse_uint8(job, &symbolic));
  D(("sftp_link %s %s [%s]", oldpath, newpath, symbolic ? "symbolic" : "hard"));
  if((symbolic ? symlink : link)(oldpath, newpath) < 0) {
    /* e.g. Linux returns EPERM for symlink or link on a FAT32 fs */
    if(errno == EPERM)
      send_status(job, SSH_FX_OP_UNSUPPORTED, 0);
    else
      send_errno_status(job);
  } else
    send_ok(job);
}

static const struct sftpcmd sftpv6tab[] = {
  { SSH_FXP_INIT, sftp_already_init },
  { SSH_FXP_OPEN, sftp_v56_open },
  { SSH_FXP_CLOSE, sftp_close },
  { SSH_FXP_READ, sftp_read },
  { SSH_FXP_WRITE, sftp_write },
  { SSH_FXP_LSTAT, sftp_v456_lstat },
  { SSH_FXP_FSTAT, sftp_v456_fstat },
  { SSH_FXP_SETSTAT, sftp_setstat },
  { SSH_FXP_FSETSTAT, sftp_fsetstat },
  { SSH_FXP_OPENDIR, sftp_opendir },
  { SSH_FXP_READDIR, sftp_readdir },
  { SSH_FXP_REMOVE, sftp_remove },
  { SSH_FXP_MKDIR, sftp_mkdir },
  { SSH_FXP_RMDIR, sftp_rmdir },
  { SSH_FXP_REALPATH, sftp_v6_realpath },
  { SSH_FXP_STAT, sftp_v456_stat },
  { SSH_FXP_RENAME, sftp_v56_rename },
  { SSH_FXP_READLINK, sftp_readlink },
  { SSH_FXP_LINK, sftp_link },
  { SSH_FXP_EXTENDED, sftp_extended }
};

/* TODO: file locking */

static const struct sftpextension sftp_v6_extensions[] = {
  { "text-seek", sftp_text_seek }
};

const struct sftpprotocol sftpv6 = {
  sizeof sftpv6tab / sizeof (struct sftpcmd),
  sftpv6tab,
  6,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_CREATETIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
   |SSH_FILEXFER_ATTR_BITS
   |SSH_FILEXFER_ATTR_LINK_COUNT),
  SSH_FX_LOCK_CONFLICT,
  v456_sendnames,
  v456_sendattrs,
  v456_parseattrs,
  v456_encode,
  v456_decode,
  sizeof sftp_v6_extensions / sizeof (struct sftpextension),
  sftp_v6_extensions,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
