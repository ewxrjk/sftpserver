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
#include "alloc.h"
#include "stat.h"
#include "parse.h"
#include "send.h"
#include "debug.h"
#include "utils.h"
#include "globals.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>

uint32_t sftp_v6_realpath(struct sftpjob *job) {
  char *path, *compose, *resolvedpath;
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
    return SSH_FX_BAD_MESSAGE;
  }
  if(!(resolvedpath = my_realpath(job->a, path, rpflags)))
    return HANDLER_ERRNO;
  D(("...real path is %s", resolvedpath));
  switch(control_byte) {
  case SSH_FXP_REALPATH_NO_CHECK:
    /* Don't stat, send dummy attributes */
    memset(&attrs, 0, sizeof attrs);
    attrs.name = resolvedpath;
    break;
  case SSH_FXP_REALPATH_STAT_IF:
    /* stat as hard as we can but accept failure if it's just not there */
    if(stat(resolvedpath, &sb) >= 0 || lstat(resolvedpath, &sb) >= 0)
      stat_to_attrs(job->a, &sb, &attrs, 0xFFFFFFFF, resolvedpath);
    else {
      memset(&attrs, 0, sizeof attrs);
      attrs.name = resolvedpath;
    }
    break;
  case SSH_FXP_REALPATH_STAT_ALWAYS:
    /* stat and error on failure */
    if(stat(resolvedpath, &sb) >= 0 || lstat(resolvedpath, &sb) >= 0)
      stat_to_attrs(job->a, &sb, &attrs, 0xFFFFFFFF, resolvedpath);
    else
      return HANDLER_ERRNO;
    break;
  }
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_NAME);
  send_uint32(job->worker, job->id);
  protocol->sendnames(job, 1, &attrs);
  send_end(job->worker);
  return HANDLER_RESPONDED;
}

uint32_t sftp_link(struct sftpjob *job) {
  char *oldpath, *newlinkpath;
  uint8_t symbolic;
  struct stat sb;

  /* See also comment in v3.c for SSH_FXP_SYMLINK */
  if(readonly)
    return SSH_FX_PERMISSION_DENIED;
  pcheck(parse_path(job, &newlinkpath));
  pcheck(parse_path(job, &oldpath));    /* aka existing-path/target-paths */
  pcheck(parse_uint8(job, &symbolic));
  D(("sftp_link %s %s [%s]", oldpath, newlinkpath,
     symbolic ? "symbolic" : "hard"));
  if((symbolic ? symlink : link)(oldpath, newlinkpath) < 0) {
    switch(errno) {
    case EPERM:
      if(!symbolic && stat(oldpath, &sb) >= 0 && S_ISDIR(sb.st_mode))
        /* Can't hard-link directories */
        return SSH_FX_FILE_IS_A_DIRECTORY;
      else
        /* e.g. Linux returns EPERM for symlink or link on a FAT32 fs */
        return SSH_FX_OP_UNSUPPORTED;
      break;
    default:
      return HANDLER_ERRNO;
    }
  } else
    return SSH_FX_OK;
}

uint32_t sftp_version_select(struct sftpjob *job) {
  char *newversion;

  /* If we've already created the work queue then this can't be the first
   * message. */
  if(!workqueue) {
    pcheck(parse_path(job, &newversion));
    /* Handle known versions */
    if(newversion[0] && !newversion[1]) {
      switch(newversion[0]) {
      case '3': protocol = &sftpv3; return SSH_FX_OK;
      case '4': protocol = &sftpv4; return SSH_FX_OK;
      case '5': protocol = &sftpv5; return SSH_FX_OK;
      case '6': protocol = &sftpv6; return SSH_FX_OK;
      }
    }
  }
  /* We're allowed to not send a response and we MUST close the channel.  (-13,
   * s5.5). */
  fatal("invalid version '%s'", newversion);
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
  { "posix-rename@openssh.org", sftp_posix_rename },
  { "space-available", sftp_space_available },
  { "statfs@openssh.org", sftp_statfs },
  { "text-seek", sftp_text_seek },
  { "version-select", sftp_version_select },
};

const struct sftpprotocol sftpv6 = {
  sizeof sftpv6tab / sizeof (struct sftpcmd),
  sftpv6tab,
  6,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_CTIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
   |SSH_FILEXFER_ATTR_BITS
   |SSH_FILEXFER_ATTR_LINK_COUNT),
  SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK,
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
