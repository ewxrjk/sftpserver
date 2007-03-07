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
#include <sys/param.h>
#include <sys/mount.h>

void sftp_v56_open(struct sftpjob *job) {
  char *path;
  uint32_t desired_access, flags;
  struct sftpattr attrs;

  pcheck(parse_path(job, &path));
  pcheck(parse_uint32(job, &desired_access));
  pcheck(parse_uint32(job, &flags));
  pcheck(protocol->parseattrs(job, &attrs));
  D(("sftp_v56_open %s %#"PRIx32" %#"PRIx32, path, desired_access, flags));
  generic_open(job, path, desired_access, flags, &attrs);
}

void generic_open(struct sftpjob *job, const char *path,
                  uint32_t desired_access, uint32_t flags,
                  struct sftpattr *attrs) {
  mode_t initial_permissions;
  int created, open_flags = 0, fd;
  struct stat sb;
  struct handleid id;
  unsigned handle_flags = 0;

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
    open_flags |= O_RDONLY;
    break;
  case ACE4_WRITE_DATA:
    open_flags |= O_WRONLY;
    break;
  case ACE4_READ_DATA|ACE4_WRITE_DATA:
    open_flags |= O_RDWR;
    break;
  }
  if(flags & (SSH_FXF_APPEND_DATA|SSH_FXF_APPEND_DATA_ATOMIC)) {
    /* We always use O_APPEND for appending so we always give atomic append. */
    open_flags |= O_APPEND;
    handle_flags |= HANDLE_APPEND;
  }
  if(flags & SSH_FXF_TEXT_MODE)
    handle_flags |= HANDLE_TEXT;
  if(flags & (SSH_FXF_BLOCK_READ|SSH_FXF_BLOCK_WRITE|SSH_FXF_BLOCK_DELETE)) {
    send_status(job, SSH_FX_OP_UNSUPPORTED,
                "SSH_FXP_OPEN locking not supported");
    return;
  }
  if(flags & SSH_FXF_NOFOLLOW) {
#ifdef O_NOFOLLOW
    /* We have a built-in way of avoiding following links */
    open_flags |= O_NOFOLLOW;
#else
    /* We avoid following links in a racy steam-driven way */
    if(lstat(path, &sb) == 0 && S_ISLNK(sb.st_mode))
      send_status(job, SSH_FX_LINK_LOOP,
                  "file is a symbolic link");
    return;
#endif
  }
  if(attrs->valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    initial_permissions = attrs->permissions & 0777;
    attrs->valid ^= SSH_FILEXFER_ATTR_PERMISSIONS;
  } else
    initial_permissions = 0777;
  switch(flags & SSH_FXF_ACCESS_DISPOSITION) {
  case SSH_FXF_CREATE_NEW:
    /* We create the file anew and if it exists we return an error. */
    if(flags & SSH_FXF_NOFOLLOW) {
      /* O_EXCL is just right if we don't want to follow links. */
      fd = open(path, open_flags|O_CREAT|O_EXCL, initial_permissions);
      created = 1;
    } else {
      /* O_EXCL will refuse to follow links so that's no good here.  We do a
       * racy test and open since that's the best available. */
      if(stat(path, &sb)) {
        send_status(job, SSH_FX_FILE_ALREADY_EXISTS, 0);
        return;
      }
      fd = open(path, open_flags|O_CREAT, initial_permissions);
      created = 1;
    }
    break;
  case SSH_FXF_CREATE_TRUNCATE:
  case SSH_FXF_OPEN_OR_CREATE:
    /* These cases differ only in whether existing files are truncated. */
    if((flags & SSH_FXF_ACCESS_DISPOSITION) == SSH_FXF_CREATE_TRUNCATE)
      open_flags |= O_TRUNC;
    if(flags & SSH_FXF_NOFOLLOW) {
      fd = open(path, open_flags|O_CREAT|O_EXCL, initial_permissions);
      if(fd >= 0)
        /* The file did not exist before */
        created = 1;
      else if(errno == EEXIST) {
        /* The file already exists.  Open and maybe truncate.  If it got
         * deleted in the meantime then you get an error. */
        fd = open(path, open_flags, initial_permissions);
        created = 0;
      } else
        created = 0;                    /* quieten compiler */
    } else {
      /* As above we cannot use O_EXCL in this case as it'll refuse to follow
       * symlinks. */
      if(stat(path, &sb)) {
        /* The file exists.  Open and maybe truncate.  If it got deleted in the
         * meantime then you get an error. */
        fd = open(path, open_flags, initial_permissions);
        created = 0;
      } else {
        /* The file does not exist yet.  If it was created in the meantime then
         * you get that file (perhaps truncated)!  We can't use O_EXCL here
         * because as observed so there is really no way round this. */
        fd = open(path, open_flags|O_CREAT, initial_permissions);
        created = 0;
      }
    }
    break;
  case SSH_FXF_OPEN_EXISTING:
    /* The file has to exist already so this case is simple. */
    fd = open(path, open_flags, initial_permissions);
    created = 0;
    break;
  case SSH_FXF_TRUNCATE_EXISTING:
    /* Again the file has to exist already so this is also simple. */
    fd = open(path, open_flags|O_TRUNC, initial_permissions);
    created = 0;
    break;
  default:
    send_status(job, SSH_FX_OP_UNSUPPORTED, "unknown flags to SSH_FXP_OPEN");
    return;
  }
  if(fd < 0) {
    send_errno_status(job);
    return;
  }
  /* Set initial attributrs if we created the file */
  if(created && attrs->valid && set_fstatus(job->a, fd, attrs)) { 
    send_errno_status(job);
    close(fd);
    unlink(path);
    return;
  }
  /* The draft says "It is implementation specific whether the directory entry
   * is removed immediately or when the handle is closed".  I interpret
   * 'immediately' to refer to the open operation. */
  if(flags & SSH_FXF_DELETE_ON_CLOSE)
    unlink(path);
  handle_new_file(&id, fd, path, handle_flags);
  D(("...handle is %"PRIu32" %"PRIu32, id.id, id.tag));
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_HANDLE);
  send_uint32(job->worker, job->id);
  send_handle(job->worker, &id);
  send_end(job->worker);
}

void sftp_v56_rename(struct sftpjob *job) {
  char *oldpath, *newpath;
  uint32_t flags;

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
      send_errno_status(job);
    else
      send_ok(job);
  } else {
    /* We want a non-overwriting rename.  We use the same strategy as
     * in v3.c. */
    if(link(oldpath, newpath) < 0) {
      if(errno != EEXIST) {
	if(rename(oldpath, newpath) < 0)
	  send_errno_status(job);
	else
	  send_ok(job);
      } else
	send_status(job, SSH_FX_FILE_ALREADY_EXISTS, 0);
    } else if(unlink(oldpath) < 0) {
      send_errno_status(job);
      unlink(newpath);
    } else
      send_ok(job);
  }
}

void sftp_text_seek(struct sftpjob *job) {
  struct handleid id;
  uint64_t line;
  int fd;
  char buffer[8192];
  ssize_t n, i;
  uint32_t rc;

  pcheck(parse_handle(job, &id));
  pcheck(parse_uint64(job, &line));
  if((rc = handle_get_fd(&id, &fd, 0, 0))) {
    send_status(job, rc, "invalid file handle");
    return;
  }
  /* Seek back to line 0 */
  if(lseek(fd, 0, SEEK_SET) < 0) {
    send_errno_status(job);
    return;
  }
  /* TODO currently if we ask for the line 'just beyond' the end of the file we
   * succeed.  We should actually return SSH_FX_EOF in this case. */
  if(line == 0) {
    /* If we're after line 0 then we're already in the right place */
    send_status(job, SSH_FX_OK, 0);
    return;
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
    send_errno_status(job);
  else if(n == 0)
    send_status(job, SSH_FX_EOF, 0);
  else {
    /* Seek back to the start of the line */
    if(lseek(fd, n - i, SEEK_CUR) < 0) {
      send_errno_status(job);
      return;
    }
    send_status(job, SSH_FX_OK, 0);
  }
}

void sftp_space_available(struct sftpjob *job) {
  char *path;
  struct statfs fs;

  pcheck(parse_string(job, &path, 0));
  D(("sftp_space_available %s", path));
  if(statfs(path, &fs) < 0) {
    send_errno_status(job);
    return;
  }
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_EXTENDED_REPLY);
  send_uint32(job->worker, job->id);
  /* bytes-on-device */
  if(fs.f_bsize >= 0 && fs.f_blocks >= 0)
    send_uint64(job->worker, (uint64_t)fs.f_bsize * (uint64_t)fs.f_blocks);
  else
    send_uint64(job->worker, 0);
  /* unused-bytes-on-device */
  if(fs.f_bsize >= 0 && fs.f_bfree >= 0)
    send_uint64(job->worker, (uint64_t)fs.f_bsize * (uint64_t)fs.f_bfree);
  else
    send_uint64(job->worker, 0);
  /* bytes-available-to-user  (i.e. both used and unused) */
  send_uint64(job->worker, 0);
  /* unused-bytes-available-to-user  (i.e. unused) */
  if(fs.f_bsize >= 0 && fs.f_bavail >= 0)
    send_uint64(job->worker, (uint64_t)fs.f_bsize * (uint64_t)fs.f_bavail);
  else
    send_uint64(job->worker, 0);
  /* bytes-per-allocation-unit */
  if(fs.f_bsize >= 0)
    send_uint32(job->worker, (uint64_t)fs.f_bsize);
  else
    send_uint32(job->worker, 0);
  send_end(job->worker);
}

void sftp_extended(struct sftpjob *job) {
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
    send_status(job, SSH_FX_OP_UNSUPPORTED,
                "extension not supported");
  else
    protocol->extensions[n].handler(job);
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
  { "text-seek", sftp_text_seek },
  { "space-available", sftp_space_available }
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
