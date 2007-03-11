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
#include "debug.h"
#include "sftp.h"
#include "handle.h"
#include "globals.h"
#include "stat.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/statvfs.h>

uint32_t sftp_v56_open(struct sftpjob *job) {
  char *path;
  uint32_t desired_access, flags;
  struct sftpattr attrs;

  pcheck(parse_path(job, &path));
  pcheck(parse_uint32(job, &desired_access));
  pcheck(parse_uint32(job, &flags));
  pcheck(protocol->parseattrs(job, &attrs));
  D(("sftp_v56_open %s %#"PRIx32" %#"PRIx32, path, desired_access, flags));
  return generic_open(job, path, desired_access, flags, &attrs);
}

uint32_t generic_open(struct sftpjob *job, const char *path,
                      uint32_t desired_access, uint32_t flags,
                      struct sftpattr *attrs) {
  mode_t initial_permissions;
  int created, open_flags = 0, fd;
  struct stat sb;
  struct handleid id;
  unsigned handle_flags = 0;

  D(("generic_open %s %#"PRIx32" %#"PRIx32, path, desired_access, flags));
  /* For opens, the size indicates the planned total size, and doesn't affect
   * the file creation. */
  attrs->valid &= ~(uint32_t)SSH_FILEXFER_ATTR_SIZE;
#ifdef O_NOCTTY
  /* We don't want to accidentally acquire a controlling terminal. */
  open_flags |= O_NOCTTY;
#endif
  switch(desired_access & (ACE4_READ_DATA|ACE4_WRITE_DATA)) {
  case 0:                               /* probably a broken client */
  case ACE4_READ_DATA:
    D(("O_RDONLY"));
    open_flags |= O_RDONLY;
    break;
  case ACE4_WRITE_DATA:
    if(readonly) return SSH_FX_PERMISSION_DENIED;
    D(("O_WRONLY"));
    open_flags |= O_WRONLY;
    break;
  case ACE4_READ_DATA|ACE4_WRITE_DATA:
    if(readonly) return SSH_FX_PERMISSION_DENIED;
    D(("O_RDWR"));
    open_flags |= O_RDWR;
    break;
  }
  if(flags & (SSH_FXF_APPEND_DATA|SSH_FXF_APPEND_DATA_ATOMIC)) {
    /* We always use O_APPEND for appending so we always give atomic append. */
    D(("O_APPEND"));
    open_flags |= O_APPEND;
    handle_flags |= HANDLE_APPEND;
  }
  if(flags & SSH_FXF_TEXT_MODE)
    handle_flags |= HANDLE_TEXT;
  if(flags & (SSH_FXF_BLOCK_READ|SSH_FXF_BLOCK_WRITE|SSH_FXF_BLOCK_DELETE))
    return SSH_FX_OP_UNSUPPORTED;
  if(flags & SSH_FXF_NOFOLLOW) {
#ifdef O_NOFOLLOW
    /* We have a built-in way of avoiding following links */
    open_flags |= O_NOFOLLOW;
    D(("O_NOFOLLOW"));
#else
    /* We avoid following links in a racy steam-driven way */
    D(("emulating O_NOFOLLOW"));
    if(lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode))
      return SSH_FX_LINK_LOOP;
#endif
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    /* If we're given permissions use them */
    initial_permissions = attrs->permissions & 07777;
    /* Don't modify permissions later unless necessary */
    if(attrs->permissions == (attrs->permissions & 0777))
      attrs->valid ^= SSH_FILEXFER_ATTR_PERMISSIONS;
    D(("using initial permission %#o", (unsigned)initial_permissions));
  } else
    /* Otherwise be conservative */
    initial_permissions = DEFAULT_PERMISSIONS & 0666;
  if(readonly
     && ((flags & SSH_FXF_ACCESS_DISPOSITION) != SSH_FXF_OPEN_EXISTING
         || (flags & SSH_FXF_DELETE_ON_CLOSE))) {
    return SSH_FX_PERMISSION_DENIED;
  }
  switch(flags & SSH_FXF_ACCESS_DISPOSITION) {
  case SSH_FXF_CREATE_NEW:
    /* We create the file anew and if it exists we return an error. */
    if(flags & SSH_FXF_NOFOLLOW) {
      /* O_EXCL is just right if we don't want to follow links. */
      D(("SSH_FXF_CREATE_NEW|SSH_FXF_NOFOLLOW -> O_CREAT|O_EXCL"));
      fd = open(path, open_flags|O_CREAT|O_EXCL, initial_permissions);
      created = 1;
    } else {
      /* O_EXCL will refuse to follow links so that's no good here.  We do a
       * racy test and open since that's the best available. */ 
      D(("SSH_FXF_CREATE_NEW -> test for existence"));
      if(stat(path, &sb) == 0) 
        return SSH_FX_FILE_ALREADY_EXISTS;
      D(("SSH_FXF_CREATE_NEW -> O_CREAT"));
      fd = open(path, open_flags|O_CREAT, initial_permissions);
      created = 1;
    }
    break;
  case SSH_FXF_CREATE_TRUNCATE:
  case SSH_FXF_OPEN_OR_CREATE:
    /* These cases differ only in whether existing files are truncated. */
    if((flags & SSH_FXF_ACCESS_DISPOSITION) == SSH_FXF_CREATE_TRUNCATE) {
      D(("SSH_FXF_CREATE_TRUNCATE -> O_TRUNC"));
      open_flags |= O_TRUNC;
    } else
      D(("SSH_FXF_OPEN_OR_TRUNCATE -> not O_TRUNC"));
    if(flags & SSH_FXF_NOFOLLOW) {
      D(("SSH_FXF_*|SSH_FXF_NOFOLLOW -> O_CREAT|O_EXCL"));
      fd = open(path, open_flags|O_CREAT|O_EXCL, initial_permissions);
      if(fd >= 0)
        /* The file did not exist before */
        created = 1;
      else if(errno == EEXIST) {
        /* The file already exists.  If it's not a link then open and maybe
         * truncate.  If it got deleted in the meantime then you get an
         * error. */
        D(("SSH_FXF_*|SSH_FXF_NOFOLLOW -> EEXIST"));
        if(lstat(path, &sb) < 0)
          return HANDLER_ERRNO;
        if(S_ISLNK(sb.st_mode))
          return SSH_FX_LINK_LOOP;
        fd = open(path, open_flags, initial_permissions);
        created = 0;
      } else
        created = 0;                    /* quieten compiler */
    } else {
      /* As above we cannot use O_EXCL in this case as it'll refuse to follow
       * symlinks. */
      D(("SSH_FXF_* -> test for existence"));
      if(stat(path, &sb) == 0) {
        /* The file exists.  Open and maybe truncate.  If it got deleted in the
         * meantime then you get an error. */
        D(("SSH_FXF_* -> open"));
        fd = open(path, open_flags, initial_permissions);
        created = 0;
      } else {
        /* The file does not exist yet.  If it was created in the meantime then
         * you get that file (perhaps truncated)!  We can't use O_EXCL here
         * because as observed so there is really no way round this. */
        D(("SSH_FXF_* -> O_CREAT"));
        fd = open(path, open_flags|O_CREAT, initial_permissions);
        created = 0;
      }
    }
    break;
  case SSH_FXF_OPEN_EXISTING:
    /* The file has to exist already so this case is simple. */
    D(("SSH_FXF_OPEN_EXISTING -> open"));
    fd = open(path, open_flags, initial_permissions);
    created = 0;
    break;
  case SSH_FXF_TRUNCATE_EXISTING:
    /* Again the file has to exist already so this is also simple - except that
     * O_NOFOLLOW doesn't inhibit following in this case. */
#ifdef O_NOFOLLOW
    D(("emulating O_NOFOLLOW"));
    if(lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode))
      return SSH_FX_LINK_LOOP;
#endif
    D(("SSH_FXF_TRUNCATE_EXISTING -> O_TRUNC"));
    fd = open(path, open_flags|O_TRUNC, initial_permissions);
    created = 0;
    break;
  default:
    return SSH_FX_OP_UNSUPPORTED;
  }
  if(fd < 0) {
#ifdef O_NOFOLLOW
    /* If we couldn't open the file because we declined to follow a symlink
     * then we need an unusual error code.  (But if we had to emulate
     * O_NOFOLLOW then the test already happened long ago.) */
    if((flags & SSH_FXF_NOFOLLOW)
       && (errno == ENOENT || errno == EEXIST)) {
      if(lstat(path, &sb) < 0)
        return HANDLER_ERRNO;
      if(S_ISLNK(sb.st_mode))
        return SSH_FX_LINK_LOOP;
    }
#endif
    return HANDLER_ERRNO;
  }
  /* Set initial attributrs if we created the file */
  if(created && attrs->valid && set_fstatus(job->a, fd, attrs)) { 
    const int save_errno = errno;
    close(fd);
    unlink(path);
    errno = save_errno;
    return HANDLER_ERRNO;
  }
  /* The draft says "It is implementation specific whether the directory entry
   * is removed immediately or when the handle is closed".  I interpret
   * 'immediately' to refer to the open operation. */
  if(flags & SSH_FXF_DELETE_ON_CLOSE) {
    D(("SSH_FXF_DELETE_ON_CLOSE"));
    unlink(path);
  }
  handle_new_file(&id, fd, path, handle_flags);
  D(("...handle is %"PRIu32" %"PRIu32, id.id, id.tag));
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_HANDLE);
  send_uint32(job->worker, job->id);
  send_handle(job->worker, &id);
  send_end(job->worker);
  return HANDLER_RESPONDED;
}

uint32_t sftp_v56_rename(struct sftpjob *job) {
  char *oldpath, *newpath;
  uint32_t flags;

  if(readonly) return SSH_FX_PERMISSION_DENIED;
  pcheck(parse_path(job, &oldpath));
  pcheck(parse_path(job, &newpath));
  pcheck(parse_uint32(job, &flags));
  D(("sftp_v56_rename %s %s %#"PRIx32, oldpath, newpath, flags));

  if(flags & (SSH_FXF_RENAME_NATIVE|SSH_FXF_RENAME_OVERWRITE)) {
    /* We asked for an overwriting rename (or a native one, in which case we're
     * allowed to do any old thing).  We are guaranteed that rename() is atomic
     * in the required sense on UNIX systems ("In this case, a link named new
     * shall remain visible to other processes throughout the renaming
     * operation and refer either to the file referred to by new or old before
     * the operation began.") so we don't bother checking the atomic bit. */
    if(rename(oldpath, newpath) < 0)
      return HANDLER_ERRNO;
    else
      return SSH_FX_OK;
  } else {
    /* We want a non-overwriting rename.  We use the same strategy as
     * in v3.c. */
    if(link(oldpath, newpath) < 0) {
      if(errno != EEXIST) {
	if(rename(oldpath, newpath) < 0)
	  return HANDLER_ERRNO;
	else
	  return SSH_FX_OK;
      } else
	return SSH_FX_FILE_ALREADY_EXISTS;
    } else if(unlink(oldpath) < 0) {
      const int save_errno = errno;

      unlink(newpath);
      errno = save_errno;
      return HANDLER_ERRNO;
    } else
      return SSH_FX_OK;
  }
}

uint32_t sftp_text_seek(struct sftpjob *job) {
  struct handleid id;
  uint64_t line;
  int fd;
  char buffer[8192];
  ssize_t n, i;
  uint32_t rc;

  pcheck(parse_handle(job, &id));
  pcheck(parse_uint64(job, &line));
  if((rc = handle_get_fd(&id, &fd, 0, 0)))
    return rc;
  /* Seek back to line 0 */
  if(lseek(fd, 0, SEEK_SET) < 0)
    return HANDLER_ERRNO;
  /* TODO currently if we ask for the line 'just beyond' the end of the file we
   * succeed.  We should actually return SSH_FX_EOF in this case. */
  if(line == 0) {
    /* If we're after line 0 then we're already in the right place */
    return SSH_FX_OK;
  }
  /* Look for the right line */
  i = 0;
  n = 0;                                /* quieten compiler */
  while(line > 0 && (n = read(fd, buffer, 0)) > 0) {
    i = 0;
    while(line > 0 && i < n) {
      if(buffer[i++] == '\n')
	--line;
    }
  }
  if(n < 0)
    return HANDLER_ERRNO;
  else if(n == 0)
    return SSH_FX_EOF;
  else {
    /* Seek back to the start of the line */
    if(lseek(fd, n - i, SEEK_CUR) < 0)
      return HANDLER_ERRNO;
    return SSH_FX_OK;
  }
}

uint32_t sftp_space_available(struct sftpjob *job) {
  char *path;
  struct statvfs fs;

  pcheck(parse_string(job, &path, 0));
  D(("sftp_space_available %s", path));
  if(statvfs(path, &fs) < 0)
    return HANDLER_ERRNO;
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_EXTENDED_REPLY);
  send_uint32(job->worker, job->id);
  /* bytes-on-device */
  send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_blocks);
  /* unused-bytes-on-device */
  send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_bfree);
  /* bytes-available-to-user  (i.e. both used and unused) */
  send_uint64(job->worker, 0);
  /* unused-bytes-available-to-user  (i.e. unused) */
  send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_bavail);
  /* bytes-per-allocation-unit */
  send_uint32(job->worker, fs.f_frsize);
  send_end(job->worker);
  return HANDLER_RESPONDED;
}

uint32_t sftp_extended(struct sftpjob *job) {
  char *name;
  int n;

  pcheck(parse_string(job, &name, 0));
  D(("extension %s", name));
  /* TODO when we have nontrivially many extensions we should use a binary
   * search here. */
  for(n = 0; (n < protocol->nextensions
	      && strcmp(name, protocol->extensions[n].name)); ++n)
    ;
  if(n >= protocol->nextensions)
    return SSH_FX_OP_UNSUPPORTED;
  else
    return protocol->extensions[n].handler(job);
}

static const struct sftpcmd sftpv5tab[] = {
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
  { SSH_FXP_REALPATH, sftp_v345_realpath },
  { SSH_FXP_STAT, sftp_v456_stat },
  { SSH_FXP_RENAME, sftp_v56_rename },
  { SSH_FXP_READLINK, sftp_readlink },
  { SSH_FXP_SYMLINK, sftp_symlink },
  { SSH_FXP_EXTENDED, sftp_extended }
};

static const struct sftpextension sftp_v5_extensions[] = {
  { "posix-rename@openssh.org", sftp_posix_rename },
  { "space-available", sftp_space_available },
  { "statfs@openssh.org", sftp_statfs },
  { "text-seek", sftp_text_seek },
};

const struct sftpprotocol sftpv5 = {
  sizeof sftpv5tab / sizeof (struct sftpcmd),
  sftpv5tab,
  5,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_CREATETIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
   |SSH_FILEXFER_ATTR_BITS),
  SSH_FX_LOCK_CONFLICT,
  v456_sendnames,
  v456_sendattrs,
  v456_parseattrs,
  v456_encode,
  v456_decode,
  sizeof sftp_v5_extensions / sizeof (struct sftpextension),
  sftp_v5_extensions,
};

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
