#include "sftpserver.h"
#include "types.h"
#include "sftp.h"
#include "parse.h"
#include "send.h"
#include "charset.h"
#include "globals.h"
#include "debug.h"
#include "stat.h"
#include "handle.h"
#include "serialize.h"
#include <string.h>

int v456_encode(struct sftpjob *job,
		char **path) {
  /* Translate local to UTF-8 */
  return iconv_wrapper(job->a, job->worker->local_to_utf8, path);
}

int v456_decode(struct sftpjob *job, 
		char **path) {
  /* Translate UTF-8 to local */
  return iconv_wrapper(job->a, job->worker->utf8_to_local, path);
}

void v456_sendattrs(struct sftpjob *job,
 		    const struct sftpattr *attrs) {
  const uint32_t valid = attrs->valid & protocol->attrbits;

  send_uint32(job, valid);
  send_uint8(job, attrs->type);
  if(valid & SSH_FILEXFER_ATTR_SIZE)
    send_uint64(job, attrs->size);
  if(valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
    send_path(job, attrs->owner);
    send_path(job, attrs->group);
  }
  if(valid & SSH_FILEXFER_ATTR_PERMISSIONS)
    send_uint32(job, attrs->permissions);
  if(valid & SSH_FILEXFER_ATTR_ACCESSTIME) {
    send_uint64(job, attrs->atime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      send_uint32(job, attrs->atime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_CREATETIME) {
    send_uint64(job, attrs->createtime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      send_uint32(job, attrs->createtime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    send_uint64(job, attrs->mtime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      send_uint32(job, attrs->mtime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_CTIME) {
    send_uint64(job, attrs->ctime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      send_uint32(job, attrs->ctime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_ACL) {
    send_string(job, attrs->acl);
  }
  if(valid & SSH_FILEXFER_ATTR_BITS) {
    send_uint32(job, attrs->attrib_bits);
    if(protocol->version >= 6)
      send_uint32(job, attrs->attrib_bits_valid);
  }
  if(valid & SSH_FILEXFER_ATTR_TEXT_HINT) {
    send_uint8(job, attrs->text_hint);
  }
  if(valid & SSH_FILEXFER_ATTR_MIME_TYPE) {
    send_string(job, attrs->mime_type);
  }
  if(valid & SSH_FILEXFER_ATTR_LINK_COUNT) {
    send_uint32(job, attrs->link_count);
  }
  /* We don't implement untranslated-name yet */
}

int v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs) {
  uint32_t n;

  memset(attrs, 0, sizeof *attrs);
  if(parse_uint32(job, &attrs->valid)) return -1;
  if(parse_uint8(job, &attrs->type)) return -1;
  if(attrs->valid & SSH_FILEXFER_ATTR_SIZE)
    if(parse_uint64(job, &attrs->size)) return -1;
  if(attrs->valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
    if(parse_path(job, &attrs->owner)) return -1;
    if(parse_path(job, &attrs->group)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    if(parse_uint32(job, &attrs->permissions)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_ACCESSTIME) {
    if(parse_uint64(job, (uint64_t *)&attrs->atime.seconds)) return -1;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if(parse_uint32(job, &attrs->atime.nanoseconds)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_CREATETIME) {
    if(parse_uint64(job, (uint64_t *)&attrs->createtime.seconds)) return -1;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if(parse_uint32(job, &attrs->createtime.nanoseconds)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    if(parse_uint64(job, (uint64_t *)&attrs->mtime.seconds)) return -1;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if(parse_uint32(job, &attrs->mtime.nanoseconds)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_CTIME) {
    if(parse_uint64(job, (uint64_t *)&attrs->ctime.seconds)) return -1;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if(parse_uint32(job, &attrs->ctime.nanoseconds)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_ACL) {
    if(parse_string(job, &attrs->acl, 0)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_BITS) {
    if(parse_uint32(job, &attrs->attrib_bits)) return -1;
    if(protocol->version >= 6) {
      if(parse_uint32(job, &attrs->attrib_bits_valid)) return -1;
    } else
      attrs->attrib_bits_valid = 0x7ff; /* -05 s5.8 */
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_TEXT_HINT) {
    if(parse_uint8(job, &attrs->text_hint)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_MIME_TYPE) {
    if(parse_string(job, &attrs->mime_type, 0)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_LINK_COUNT) {
    if(parse_uint32(job, &attrs->link_count)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_UNTRANSLATED_NAME) {
    if(parse_string(job, &attrs->mime_type, 0)) return -1;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_EXTENDED) {
    if(parse_uint32(job, &n)) return -1;
    while(n-- > 0) {
      parse_string(job, 0, 0);
      parse_string(job, 0, 0);
    }
  }
  return 0;
}

/* Send a filename list as found in an SSH_FXP_NAME response.  The response
 * header and so on must be generated by the caller. */
void v456_sendnames(struct sftpjob *job, 
		    int nnames, const struct sftpattr *names) {
  /* We'd like to know what year we're in for dates in longname */
  send_uint32(job, nnames);
  while(nnames > 0) {
    send_path(job, names->name);
    protocol->sendattrs(job, names);
    ++names;
    --nnames;
  }
}

/* Command code for the various _*STAT calls.  rc is the return value
 * from *stat() and SB is the buffer. */
static void sftp_v456_stat_core(struct sftpjob *job, int rc, 
				const struct stat *sb) {
  struct sftpattr attrs;
  uint32_t flags;

  if(!rc) {
    pcheck(parse_uint32(job, &flags));
    stat_to_attrs(job->a, sb, &attrs, flags);
    send_begin(job);
    send_uint8(job, SSH_FXP_ATTRS);
    send_uint32(job, job->id);
    protocol->sendattrs(job, &attrs);
    send_end(job);
  } else
    send_errno_status(job);
}

void sftp_v456_lstat(struct sftpjob *job) {
  char *path;
  struct stat sb;

  pcheck(parse_path(job, &path));
  D(("sftp_lstat %s", path));
  sftp_v456_stat_core(job, lstat(path, &sb), &sb);
}

void sftp_v456_stat(struct sftpjob *job) {
  char *path;
  struct stat sb;

  pcheck(parse_path(job, &path));
  D(("sftp_stat %s", path));
  sftp_v456_stat_core(job, stat(path, &sb), &sb);
}

void sftp_v456_fstat(struct sftpjob *job) {
  int fd;
  struct handleid id;
  struct stat sb;
  uint32_t rc;

  pcheck(parse_handle(job, &id));
  D(("sftp_fstat %"PRIu32" %"PRIu32, id.id, id.tag));
  if((rc = handle_get_fd(&id, &fd, 0, 0))) {
    send_status(job, rc, "invalid file handle");
    return;
  }
  serialize_on_handle(job, 0);
  sftp_v456_stat_core(job, fstat(fd, &sb), &sb);
}

static const struct sftpcmd sftpv4tab[] = {
  { SSH_FXP_INIT, sftp_already_init },
  { SSH_FXP_OPEN, sftp_v34_open },
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
  { SSH_FXP_REALPATH, sftp_v345_realpath },
  { SSH_FXP_STAT, sftp_v456_stat },
  { SSH_FXP_RENAME, sftp_v34_rename },
  { SSH_FXP_READLINK, sftp_readlink },
  { SSH_FXP_SYMLINK, sftp_symlink },
  //{ SSH_FXP_EXTENDED, sftp_extended },
};

const struct sftpprotocol sftpv4 = {
  sizeof sftpv4tab / sizeof (struct sftpcmd),
  sftpv4tab,
  4,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_CREATETIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES),
  SSH_FX_NO_MEDIA,
  v456_sendnames,
  v456_sendattrs,
  v456_parseattrs,
  v456_encode,
  v456_decode
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
