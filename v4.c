/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

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

int sftp_v456_encode(struct sftpjob *job,
		char **path) {
  /* Translate local to UTF-8 */
  return sftp_iconv(job->a, job->worker->local_to_utf8, path);
}

uint32_t sftp_v456_decode(struct sftpjob *job, 
                     char **path) {
  /* Translate UTF-8 to local */
  if(sftp_iconv(job->a, job->worker->utf8_to_local, path))
    return SSH_FX_INVALID_FILENAME;
  else
    return SSH_FX_OK;
}

void sftp_v456_sendattrs(struct sftpjob *job,
 		    const struct sftpattr *attrs) {
  const uint32_t valid = attrs->valid & protocol->attrmask;

  sftp_send_uint32(job->worker, valid);
  sftp_send_uint8(job->worker, attrs->type);
  if(valid & SSH_FILEXFER_ATTR_SIZE)
    sftp_send_uint64(job->worker, attrs->size);
  if(valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
    sftp_send_path(job, job->worker, attrs->owner);
    sftp_send_path(job, job->worker, attrs->group);
  }
  if(valid & SSH_FILEXFER_ATTR_PERMISSIONS)
    sftp_send_uint32(job->worker, attrs->permissions);
  /* lftp 3.1.3 expects subsecond time fields even if the corresonding time is
   * absent.  It sends no identifying information so we cannot enable a
   * workaround for this bizarre bug.  lftp 3.5.9 gets this right as does
   * WinSCP. */
  if(valid & SSH_FILEXFER_ATTR_ACCESSTIME) {
    sftp_send_uint64(job->worker, attrs->atime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      sftp_send_uint32(job->worker, attrs->atime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_CREATETIME) {
    sftp_send_uint64(job->worker, attrs->createtime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      sftp_send_uint32(job->worker, attrs->createtime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    sftp_send_uint64(job->worker, attrs->mtime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      sftp_send_uint32(job->worker, attrs->mtime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_CTIME) {
    sftp_send_uint64(job->worker, attrs->ctime.seconds);
    if(valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      sftp_send_uint32(job->worker, attrs->ctime.nanoseconds);
  }
  if(valid & SSH_FILEXFER_ATTR_ACL) {
    sftp_send_string(job->worker, attrs->acl);
  }
  if(valid & SSH_FILEXFER_ATTR_BITS) {
    sftp_send_uint32(job->worker, attrs->attrib_bits);
    if(protocol->version >= 6)
      sftp_send_uint32(job->worker, attrs->attrib_bits_valid);
  }
  if(valid & SSH_FILEXFER_ATTR_TEXT_HINT) {
    sftp_send_uint8(job->worker, attrs->text_hint);
  }
  if(valid & SSH_FILEXFER_ATTR_MIME_TYPE) {
    sftp_send_string(job->worker, attrs->mime_type);
  }
  if(valid & SSH_FILEXFER_ATTR_LINK_COUNT) {
    sftp_send_uint32(job->worker, attrs->link_count);
  }
  /* We don't implement untranslated-name yet */
}

uint32_t sftp_v456_parseattrs(struct sftpjob *job, struct sftpattr *attrs) {
  uint32_t n, rc;

  memset(attrs, 0, sizeof *attrs);
  if((rc = sftp_parse_uint32(job, &attrs->valid)) != SSH_FX_OK)
    return rc;
  if((rc = sftp_parse_uint8(job, &attrs->type)) != SSH_FX_OK)
    return rc;
  if(attrs->valid & SSH_FILEXFER_ATTR_SIZE)
    if((rc = sftp_parse_uint64(job, &attrs->size)) != SSH_FX_OK)
      return rc;
  if(attrs->valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
    if((rc = sftp_parse_path(job, &attrs->owner)) != SSH_FX_OK)
      return rc;
    if((rc = sftp_parse_path(job, &attrs->group)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    if((rc = sftp_parse_uint32(job, &attrs->permissions)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_ACCESSTIME) {
    if((rc = sftp_parse_uint64(job, (uint64_t *)&attrs->atime.seconds)) != SSH_FX_OK)
      return rc;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if((rc = sftp_parse_uint32(job, &attrs->atime.nanoseconds)) != SSH_FX_OK)
        return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_CREATETIME) {
    if((rc = sftp_parse_uint64(job, (uint64_t *)&attrs->createtime.seconds)) != SSH_FX_OK)
      return rc;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if((rc = sftp_parse_uint32(job, &attrs->createtime.nanoseconds)) != SSH_FX_OK)
        return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    if((rc = sftp_parse_uint64(job, (uint64_t *)&attrs->mtime.seconds)) != SSH_FX_OK)
      return rc;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if((rc = sftp_parse_uint32(job, &attrs->mtime.nanoseconds)) != SSH_FX_OK)
        return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_CTIME) {
    if((rc = sftp_parse_uint64(job, (uint64_t *)&attrs->ctime.seconds)) != SSH_FX_OK)
      return rc;
    if(attrs->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES)
      if((rc = sftp_parse_uint32(job, &attrs->ctime.nanoseconds)) != SSH_FX_OK)
        return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_ACL) {
    if((rc = sftp_parse_string(job, &attrs->acl, 0)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_BITS) {
    if((rc = sftp_parse_uint32(job, &attrs->attrib_bits)) != SSH_FX_OK)
      return rc;
    if(protocol->version >= 6) {
      if((rc = sftp_parse_uint32(job, &attrs->attrib_bits_valid)) != SSH_FX_OK)
        return rc;
    } else
      attrs->attrib_bits_valid = 0x7ff; /* -05 s5.8 */
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_TEXT_HINT) {
    if((rc = sftp_parse_uint8(job, &attrs->text_hint)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_MIME_TYPE) {
    if((rc = sftp_parse_string(job, &attrs->mime_type, 0)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_LINK_COUNT) {
    if((rc = sftp_parse_uint32(job, &attrs->link_count)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_UNTRANSLATED_NAME) {
    if((rc = sftp_parse_string(job, &attrs->mime_type, 0)) != SSH_FX_OK)
      return rc;
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_EXTENDED) {
    if((rc = sftp_parse_uint32(job, &n)) != SSH_FX_OK)
      return rc;
    while(n-- > 0) {
      if((rc = sftp_parse_string(job, 0, 0)) != SSH_FX_OK)
        return rc;
      if((rc = sftp_parse_string(job, 0, 0)) != SSH_FX_OK)
        return rc;
    }
  }
  return SSH_FX_OK;
}

/* Send a filename list as found in an SSH_FXP_NAME response.  The response
 * header and so on must be generated by the caller. */
void sftp_v456_sendnames(struct sftpjob *job, 
		    int nnames, const struct sftpattr *names) {
  /* We'd like to know what year we're in for dates in longname */
  sftp_send_uint32(job->worker, nnames);
  while(nnames > 0) {
    sftp_send_path(job, job->worker, names->name);
    protocol->sendattrs(job, names);
    ++names;
    --nnames;
  }
}

/* Command code for the various _*STAT calls.  rc is the return value
 * from *stat() and SB is the buffer. */
static uint32_t sftp_v456_stat_core(struct sftpjob *job, int rc, 
                                    const struct stat *sb, const char *path) {
  struct sftpattr attrs;
  uint32_t flags;

  if(!rc) {
    pcheck(sftp_parse_uint32(job, &flags));
    sftp_stat_to_attrs(job->a, sb, &attrs, flags, path);
    sftp_send_begin(job->worker);
    sftp_send_uint8(job->worker, SSH_FXP_ATTRS);
    sftp_send_uint32(job->worker, job->id);
    protocol->sendattrs(job, &attrs);
    sftp_send_end(job->worker);
    return HANDLER_RESPONDED;
  } else
    return HANDLER_ERRNO;
}

uint32_t sftp_v456_lstat(struct sftpjob *job) {
  char *path;
  struct stat sb;

  pcheck(sftp_parse_path(job, &path));
  D(("sftp_lstat %s", path));
  return sftp_v456_stat_core(job, lstat(path, &sb), &sb, path);
}

uint32_t sftp_v456_stat(struct sftpjob *job) {
  char *path;
  struct stat sb;

  pcheck(sftp_parse_path(job, &path));
  D(("sftp_stat %s", path));
  return sftp_v456_stat_core(job, stat(path, &sb), &sb, path);
}

uint32_t sftp_v456_fstat(struct sftpjob *job) {
  int fd;
  struct handleid id;
  struct stat sb;
  uint32_t rc;

  pcheck(sftp_parse_handle(job, &id));
  D(("sftp_fstat %"PRIu32" %"PRIu32, id.id, id.tag));
  if((rc = sftp_handle_get_fd(&id, &fd, 0)))
    return rc;
  return sftp_v456_stat_core(job, fstat(fd, &sb), &sb, 0);
}

static const struct sftpcmd sftpv4tab[] = {
  { SSH_FXP_INIT, sftp_vany_already_init },
  { SSH_FXP_OPEN, sftp_v34_open },
  { SSH_FXP_CLOSE, sftp_vany_close },
  { SSH_FXP_READ, sftp_vany_read },
  { SSH_FXP_WRITE, sftp_vany_write },
  { SSH_FXP_LSTAT, sftp_v456_lstat },
  { SSH_FXP_FSTAT, sftp_v456_fstat },
  { SSH_FXP_SETSTAT, sftp_vany_setstat },
  { SSH_FXP_FSETSTAT, sftp_vany_fsetstat },
  { SSH_FXP_OPENDIR, sftp_vany_opendir },
  { SSH_FXP_READDIR, sftp_vany_readdir },
  { SSH_FXP_REMOVE, sftp_vany_remove },
  { SSH_FXP_MKDIR, sftp_vany_mkdir },
  { SSH_FXP_RMDIR, sftp_vany_rmdir },
  { SSH_FXP_REALPATH, sftp_v345_realpath },
  { SSH_FXP_STAT, sftp_v456_stat },
  { SSH_FXP_RENAME, sftp_v34_rename },
  { SSH_FXP_READLINK, sftp_vany_readlink },
  { SSH_FXP_SYMLINK, sftp_v345_symlink },
  { SSH_FXP_EXTENDED, sftp_vany_extended }
};

static const struct sftpextension v4_extensions[] = {
  { "posix-rename@openssh.org", sftp_vany_posix_rename },
  { "space-available", sftp_vany_space_available },
  { "statfs@openssh.org", sftp_vany_statfs },
  { "text-seek", sftp_vany_text_seek },
};

const struct sftpprotocol sftp_v4 = {
  sizeof sftpv4tab / sizeof (struct sftpcmd),
  sftpv4tab,
  4,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES),
  SSH_FX_NO_MEDIA,
  sftp_v456_sendnames,
  sftp_v456_sendattrs,
  sftp_v456_parseattrs,
  sftp_v456_encode,
  sftp_v456_decode,
  sizeof v4_extensions / sizeof (struct sftpextension),
  v4_extensions,                        /* extensions */
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
