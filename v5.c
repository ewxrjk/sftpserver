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
#include "utils.h"
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

  pcheck(sftp_parse_path(job, &path));
  pcheck(sftp_parse_uint32(job, &desired_access));
  pcheck(sftp_parse_uint32(job, &flags));
  pcheck(protocol->parseattrs(job, &attrs));
  D(("sftp_v56_open %s %#"PRIx32" %#"PRIx32, path, desired_access, flags));
  return sftp_generic_open(job, path, desired_access, flags, &attrs);
}

uint32_t sftp_generic_open(struct sftpjob *job, const char *path,
                      uint32_t desired_access, uint32_t flags,
                      struct sftpattr *attrs) {
  mode_t initial_permissions;
  int created, open_flags, fd;
  struct stat sb;
  struct handleid id;
  unsigned sftp_handle_flags = 0;
  uint32_t rc;

  D(("sftp_generic_open %s %#"PRIx32" %#"PRIx32, path, desired_access, flags));
  /* Check owner/group */
  if((rc = sftp_normalize_ownergroup(job->a, attrs)) != SSH_FX_OK)
    return rc;
  /* For opens, the size indicates the planned total size, and doesn't affect
   * the file creation. */
  attrs->valid &= ~(uint32_t)SSH_FILEXFER_ATTR_SIZE;
  /* The v6 spec says implementations "SHOULD" pre-allocate according to the
   * allocation-size field if present.  This is impossible on UNIX so we ignore
   * it.
   *
   * This means that the MUST later in the same section, that the queried
   * allocation-size must exceed the requested one, cannot be honored.  Since
   * the specification taken literally prohibits UNIX implementations, I assume
   * that it is in error.
   */
  switch(desired_access & (ACE4_READ_DATA|ACE4_WRITE_DATA)) {
  case 0:                               /* probably a broken client */
  case ACE4_READ_DATA:
    D(("O_RDONLY"));
    open_flags = O_RDONLY;
    if(desired_access & (ACE4_WRITE_NAMED_ATTRS
                         |ACE4_WRITE_ATTRIBUTES
                         |ACE4_WRITE_ACL
                         |ACE4_WRITE_OWNER))
      return SSH_FX_PERMISSION_DENIED;
    break;
  case ACE4_WRITE_DATA:
    if(readonly)
      return SSH_FX_PERMISSION_DENIED;
    D(("O_WRONLY"));
    open_flags = O_WRONLY;
    if(desired_access & (ACE4_READ_NAMED_ATTRS
                         |ACE4_READ_ATTRIBUTES
                         |ACE4_READ_ACL))
      return SSH_FX_PERMISSION_DENIED;
    break;
  case ACE4_READ_DATA|ACE4_WRITE_DATA:
    if(readonly)
      return SSH_FX_PERMISSION_DENIED;
    D(("O_RDWR"));
    open_flags = O_RDWR;
    break;
  default:
    fatal("bitwise operators have broken");
  }
  /* UNIX systems generally do not allow the owner to be changed by
   * non-superusers. */
  if((desired_access & ACE4_WRITE_OWNER) && getuid())
    return SSH_FX_PERMISSION_DENIED;
  /* LIST_DIRECTORY, ADD_FILE, ADD_SUBDIRECTORY and DELETE_CHILD don't make
   * sense for regular files.  We ignore them, regarding a grant of permission
   * as not implying that the operation can possibly work.
   *
   * Similary SYNCHRONIZE and EXECUTE we're happy to grant permission for even
   * if there's no way to take advantage of that permission.
   *
   * DELETE is always possible and nothing to do with opening the file.
   */
#ifdef O_NOCTTY
  /* We don't want to accidentally acquire a controlling terminal. */
  open_flags |= O_NOCTTY;
#endif
  if(flags & (SSH_FXF_APPEND_DATA|SSH_FXF_APPEND_DATA_ATOMIC)) {
    /* We always use O_APPEND for appending so we always give atomic append. */
    D(("O_APPEND"));
    open_flags |= O_APPEND;
    sftp_handle_flags |= HANDLE_APPEND;
  }
  if(flags & SSH_FXF_TEXT_MODE)
    sftp_handle_flags |= HANDLE_TEXT;
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
    /* The file has to exist already so this case is simple.  O_NOFOLLOW
     * doesn't work reliably in this case. */
#ifdef O_NOFOLLOW
    if(flags & SSH_FXF_NOFOLLOW) {
      D(("emulating O_NOFOLLOW"));
      if(lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode))
        return SSH_FX_LINK_LOOP;
    }
#endif
    D(("SSH_FXF_OPEN_EXISTING -> open"));
    fd = open(path, open_flags, initial_permissions);
    created = 0;
    break;
  case SSH_FXF_TRUNCATE_EXISTING:
    /* Again the file has to exist already so this is also simple - except that
     * O_NOFOLLOW doesn't inhibit following in this case. */
#ifdef O_NOFOLLOW
    if(flags & SSH_FXF_NOFOLLOW) {
      D(("emulating O_NOFOLLOW"));
      if(lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode))
        return SSH_FX_LINK_LOOP;
    }
#endif
    D(("SSH_FXF_TRUNCATE_EXISTING -> O_TRUNC"));
    fd = open(path, open_flags|O_TRUNC, initial_permissions);
    created = 0;
    break;
  default:
    return SSH_FX_OP_UNSUPPORTED;
  }
  /* BSD lets us open directories, but we MUST return an error */
  if(fd >= 0 && fstat(fd, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    close(fd);
    return SSH_FX_FILE_IS_A_DIRECTORY;
  }
  if(fd < 0) {
    if(((flags & SSH_FXF_ACCESS_DISPOSITION) == SSH_FXF_OPEN_EXISTING
        || (flags & SSH_FXF_ACCESS_DISPOSITION) == SSH_FXF_TRUNCATE_EXISTING)
       && errno == ENOENT) {
      /* The client could do the extra check but at the cost of a round trip,
       * so it makes some sense to do it and return the alternative status.
       * This is of rather limited use as the client has no way to know that we
       * reliably send SSH_FX_NO_SUCH_PATH when appropriate, but the
       * alternative error message might clue in users occasionally. */
      if(lstat(sftp_dirname(job->a, path), &sb) < 0)
        return SSH_FX_NO_SUCH_PATH;
      errno = ENOENT;                   /* restore errno just in case */
    }
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
  if(created && attrs->valid && (rc = sftp_set_fstatus(job->a, fd, attrs, 0))) { 
    const int save_errno = errno;
    close(fd);
    unlink(path);
    errno = save_errno;
    return rc;
  }
  /* The draft says "It is implementation specific whether the directory entry
   * is removed immediately or when the handle is closed".  I interpret
   * 'immediately' to refer to the open operation. */
  if(flags & SSH_FXF_DELETE_ON_CLOSE) {
    D(("SSH_FXF_DELETE_ON_CLOSE"));
    unlink(path);
  }
  sftp_handle_new_file(&id, fd, path, sftp_handle_flags);
  D(("...handle is %"PRIu32" %"PRIu32, id.id, id.tag));
  sftp_send_begin(job->worker);
  sftp_send_uint8(job->worker, SSH_FXP_HANDLE);
  sftp_send_uint32(job->worker, job->id);
  sftp_send_handle(job->worker, &id);
  sftp_send_end(job->worker);
  return HANDLER_RESPONDED;
}

uint32_t sftp_v56_rename(struct sftpjob *job) {
  char *oldpath, *newpath;
  uint32_t flags;

  if(readonly)
    return SSH_FX_PERMISSION_DENIED;
  pcheck(sftp_parse_path(job, &oldpath));
  pcheck(sftp_parse_path(job, &newpath));
  pcheck(sftp_parse_uint32(job, &flags));
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
#ifndef __linux__
        {
          struct stat sb;
          
          if(lstat(newpath, &sb) == 0)
            return SSH_FX_FILE_ALREADY_EXISTS;
        }
#endif
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

uint32_t sftp_vany_text_seek(struct sftpjob *job) {
  struct handleid id;
  uint64_t line;
  int fd;
  char buffer[8192];
  ssize_t n, i;
  uint32_t rc;

  pcheck(sftp_parse_handle(job, &id));
  pcheck(sftp_parse_uint64(job, &line));
  if((rc = sftp_handle_get_fd(&id, &fd, 0)))
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
  D(("on entry: line=%"PRIu64, line));
  while(line > 0 && (n = read(fd, buffer, sizeof buffer)) > 0) {
    D(("outer: line=%"PRIu64" n=%zd", line, n));
    i = 0;
    while(line > 0 && i < n) {
      if(buffer[i++] == '\n')
	--line;
    }
  }
  D(("exited:: line=%"PRIu64" n=%zd i=%zd", line, n, i));
  if(n < 0)
    return HANDLER_ERRNO;
  else if(n == 0)
    return SSH_FX_EOF;
  else {
    /* Seek back to the start of the line */
    if(lseek(fd, i - n, SEEK_CUR) < 0)
      return HANDLER_ERRNO;
    return SSH_FX_OK;
  }
}

uint32_t sftp_vany_space_available(struct sftpjob *job) {
  char *path;
  struct statvfs fs;

  pcheck(sftp_parse_string(job, &path, 0));
  D(("sftp_space_available %s", path));
  if(statvfs(path, &fs) < 0)
    return HANDLER_ERRNO;
  sftp_send_begin(job->worker);
  sftp_send_uint8(job->worker, SSH_FXP_EXTENDED_REPLY);
  sftp_send_uint32(job->worker, job->id);
  /* bytes-on-device */
  sftp_send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_blocks);
  /* unused-bytes-on-device */
  sftp_send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_bfree);
  /* bytes-available-to-user  (i.e. both used and unused) */
  sftp_send_uint64(job->worker, 0);
  /* unused-bytes-available-to-user  (i.e. unused) */
  sftp_send_uint64(job->worker, (uint64_t)fs.f_frsize * (uint64_t)fs.f_bavail);
  /* bytes-per-allocation-unit */
  sftp_send_uint32(job->worker, fs.f_frsize);
  sftp_send_end(job->worker);
  return HANDLER_RESPONDED;
}

uint32_t sftp_vany_extended(struct sftpjob *job) {
  char *name;
  int n;

  pcheck(sftp_parse_string(job, &name, 0));
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
  { SSH_FXP_INIT, sftp_vany_already_init },
  { SSH_FXP_OPEN, sftp_v56_open },
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
  { SSH_FXP_RENAME, sftp_v56_rename },
  { SSH_FXP_READLINK, sftp_vany_readlink },
  { SSH_FXP_SYMLINK, sftp_v345_symlink },
  { SSH_FXP_EXTENDED, sftp_vany_extended }
};

static const struct sftpextension sftp_v5_extensions[] = {
  { "posix-rename@openssh.org", sftp_vany_posix_rename },
  { "space-available", sftp_vany_space_available },
  { "statfs@openssh.org", sftp_vany_statfs },
  { "text-seek", sftp_vany_text_seek },
};

const struct sftpprotocol sftp_v5 = {
  sizeof sftpv5tab / sizeof (struct sftpcmd),
  sftpv5tab,
  5,
  (SSH_FILEXFER_ATTR_SIZE
   |SSH_FILEXFER_ATTR_PERMISSIONS
   |SSH_FILEXFER_ATTR_ACCESSTIME
   |SSH_FILEXFER_ATTR_MODIFYTIME
   |SSH_FILEXFER_ATTR_OWNERGROUP
   |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
   |SSH_FILEXFER_ATTR_BITS),
  SSH_FX_LOCK_CONFLICT,
  sftp_v456_sendnames,
  sftp_v456_sendattrs,
  sftp_v456_parseattrs,
  sftp_v456_encode,
  sftp_v456_decode,
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
