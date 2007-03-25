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
#include "users.h"
#include "sftp.h"
#include "alloc.h"
#include "debug.h"
#include "stat.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#if ! HAVE_FUTIMES
int futimes(int fd, const struct timeval *times);
#endif

void sftp_stat_to_attrs(struct allocator *a,
		   const struct stat *sb, struct sftpattr *attrs,
                   uint32_t flags, const char *path) {
  memset(attrs, 0, sizeof *attrs);
  attrs->valid = (SSH_FILEXFER_ATTR_SIZE
		  |SSH_FILEXFER_ATTR_PERMISSIONS
		  |SSH_FILEXFER_ATTR_ACCESSTIME
		  |SSH_FILEXFER_ATTR_MODIFYTIME
		  |SSH_FILEXFER_ATTR_UIDGID
		  |SSH_FILEXFER_ATTR_ALLOCATION_SIZE
		  |SSH_FILEXFER_ATTR_LINK_COUNT
		  |SSH_FILEXFER_ATTR_CTIME
                  |SSH_FILEXFER_ATTR_BITS);
  switch(sb->st_mode & S_IFMT) {
  case S_IFIFO: attrs->type = SSH_FILEXFER_TYPE_FIFO; break;
  case S_IFCHR: attrs->type = SSH_FILEXFER_TYPE_CHAR_DEVICE; break;
  case S_IFDIR: attrs->type = SSH_FILEXFER_TYPE_DIRECTORY; break;
  case S_IFBLK: attrs->type = SSH_FILEXFER_TYPE_BLOCK_DEVICE; break;
  case S_IFREG: attrs->type = SSH_FILEXFER_TYPE_REGULAR; break;
  case S_IFLNK: attrs->type = SSH_FILEXFER_TYPE_SYMLINK; break;
  case S_IFSOCK: attrs->type = SSH_FILEXFER_TYPE_SOCKET; break;
  default: attrs->type = SSH_FILEXFER_TYPE_SPECIAL; break;
  }
  attrs->size = sb->st_size;
  /* The cast ensures we don't do the multiply in a small word and overflow */
  attrs->allocation_size = (uint64_t)sb->st_blksize * sb->st_blocks;
  /* Only look up owner/group info if wanted */
  if((flags & SSH_FILEXFER_ATTR_OWNERGROUP)
     && (attrs->owner = sftp_uid2name(a, sb->st_uid))
     && (attrs->group = sftp_gid2name(a, sb->st_gid)))
    attrs->valid |= SSH_FILEXFER_ATTR_OWNERGROUP;
  attrs->uid = sb->st_uid;
  attrs->gid = sb->st_gid;
  attrs->permissions = sb->st_mode;
  attrs->atime.seconds = sb->st_atime;
  attrs->mtime.seconds = sb->st_mtime;
  attrs->ctime.seconds = sb->st_ctime;
#if HAVE_STAT_TIMESPEC
  if(sb->st_atimespec.tv_nsec >= 0
     && sb->st_atimespec.tv_nsec < 1000000000
     && sb->st_mtimespec.tv_nsec >= 0
     && sb->st_mtimespec.tv_nsec < 1000000000
     && sb->st_ctimespec.tv_nsec >= 0
     && sb->st_ctimespec.tv_nsec < 1000000000) {
    /* Only send subsecond times if they are in range */
    attrs->atime.nanoseconds = sb->st_atimespec.tv_nsec;
    attrs->mtime.nanoseconds = sb->st_mtimespec.tv_nsec;
    attrs->ctime.nanoseconds = sb->st_ctimespec.tv_nsec;
    attrs->valid |= SSH_FILEXFER_ATTR_SUBSECOND_TIMES;
  }
#endif
  attrs->link_count = sb->st_nlink;
  /* If we know the path we can determine whether the file is hidden or not */
  if(path) {
    const char *s = path + strlen(path);
    /* Ignore trailing slashes */
    while(s > path && s[-1] == '/')
      --s;
    while(s > path && s[-1] != '/')
      --s;
    if(*s == '.')
      attrs->attrib_bits |= SSH_FILEXFER_ATTR_FLAGS_HIDDEN;
    attrs->attrib_bits_valid |= SSH_FILEXFER_ATTR_FLAGS_HIDDEN;
  }
  /* TODO: SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE for directories on
   * case-independent filesystems */
  /* TODO: SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE for immutable files.  Perhaps when
   * I can find some documentation on the subject. */
  attrs->name = path;
}

static const struct {
  uint32_t bit;
  const char *description;
} attr_bits[] = {
  { SSH_FILEXFER_ATTR_FLAGS_READONLY, "ro" },
  { SSH_FILEXFER_ATTR_FLAGS_SYSTEM, "sys" },
  { SSH_FILEXFER_ATTR_FLAGS_HIDDEN, "hide" },
  { SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE, "ci"},
  { SSH_FILEXFER_ATTR_FLAGS_ARCHIVE, "arc" },
  { SSH_FILEXFER_ATTR_FLAGS_ENCRYPTED, "enc" },
  { SSH_FILEXFER_ATTR_FLAGS_COMPRESSED, "comp" },
  { SSH_FILEXFER_ATTR_FLAGS_SPARSE, "sp" },
  { SSH_FILEXFER_ATTR_FLAGS_APPEND_ONLY, "app" },
  { SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE, "imm" },
  { SSH_FILEXFER_ATTR_FLAGS_SYNC, "sync" },
  { SSH_FILEXFER_ATTR_FLAGS_TRANSLATION_ERR, "trans" }
};

const char *sftp_format_attr(struct allocator *a,
			const struct sftpattr *attrs, int thisyear,
			unsigned long flags) {
  char perms[64], linkcount[64], size[64], date[64], nowner[64], ngroup[64];
  char *formatted, *p, bits[64];
  const char *owner, *group;
  static const char typedetails[] = "?-dl??scbp";
  size_t n;

  /* permissions */
  p = perms;
  *p++ = typedetails[attrs->type];
  if(attrs->valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    *p++ = (attrs->permissions & 00400) ? 'r' : '-';
    *p++ = (attrs->permissions & 00200) ? 'w' : '-';
    switch(attrs->permissions & 04100) {
    case 00000: *p++ = '-'; break;
    case 00100: *p++ = 'x'; break;
    case 04000: *p++ = 'S'; break;
    case 04100: *p++ = 's'; break;
    }
    *p++ = (attrs->permissions & 00040) ? 'r' : '-';
    *p++ = (attrs->permissions & 00020) ? 'w' : '-';
    switch(attrs->permissions & 02010) {
    case 00000: *p++ = '-'; break;
    case 00010: *p++ = 'x'; break;
    case 02000: *p++ = 'S'; break;
    case 02010: *p++ = 's'; break;
    }
    *p++ = (attrs->permissions & 00004) ? 'r' : '-';
    *p++ = (attrs->permissions & 00002) ? 'w' : '-';
    switch(attrs->permissions & 01001) {
    case 00000: *p++ = '-'; break;
    case 00001: *p++ = 'x'; break;
    case 01000: *p++ = 'T'; break;
    case 01001: *p++ = 't'; break;
    }
    *p = 0;
  } else
    strcpy(p, "?????????");
  /* link count */
  if(attrs->valid & SSH_FILEXFER_ATTR_LINK_COUNT)
    sprintf(linkcount, "%"PRIu32, attrs->link_count);
  else
    strcpy(linkcount, "?");
  /* size */
  if(attrs->valid & SSH_FILEXFER_ATTR_SIZE)
    sprintf(size, "%"PRIu64, attrs->size);
  else
    strcpy(size, "?");
  /* ownership */
  if(attrs->valid & SSH_FILEXFER_ATTR_UIDGID) {
    sprintf(nowner, "%"PRIu32, attrs->uid);
    sprintf(ngroup, "%"PRIu32, attrs->gid);
  }
  owner = group = "?";
  if(flags & FORMAT_PREFER_NUMERIC_UID) {
    if(attrs->valid & SSH_FILEXFER_ATTR_UIDGID) {
      owner = nowner;
      group = ngroup;
    } else if(attrs->valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
      owner = attrs->owner;
      group = attrs->group;
    }
  } else {
    if(attrs->valid & SSH_FILEXFER_ATTR_OWNERGROUP) {
      owner = attrs->owner;
      group = attrs->group;
    } else if(attrs->valid & SSH_FILEXFER_ATTR_UIDGID) {
      owner = nowner;
      group = ngroup;
    }
  }
  /* timestamp */
  if(attrs->valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    struct tm mtime;
    const time_t m = attrs->mtime.seconds;
    (flags & FORMAT_PREFER_LOCALTIME ? localtime_r : gmtime_r)(&m, &mtime);
    /* Timestamps in the current year we give down to seconds.  For
     * timestamps in other years we give the year. */
    if(mtime.tm_year == thisyear)
      strftime(date, sizeof date, "%b %d %H:%M", &mtime);
    else
      strftime(date, sizeof date, "%b %d  %Y", &mtime);
  } else
    strcpy(date, "?");
  /* attribute bits */
  bits[0] = 0;
  if(flags & FORMAT_ATTRS
     && (attrs->valid & SSH_FILEXFER_ATTR_BITS)
     && attrs->attrib_bits) {
    strcat(bits, "[");
    for(n = 0; n < sizeof attr_bits / sizeof *attr_bits; ++n) {
      if(attrs->attrib_bits & attr_bits[n].bit) {
        if(bits[1])
          strcat(bits, ",");
        strcat(bits, attr_bits[n].description);
      }
    }
    strcat(bits, "]");
  }
  /* Format the result */
  formatted = sftp_alloc(a, 80 + strlen(attrs->name));

  /* The draft is pretty specific about field widths */
  sprintf(formatted, "%10.10s %3.3s %-8.8s %-8.8s %8.8s %12.12s %s%s%s%s%s",
	  perms, linkcount, owner, group,
	  size, date, attrs->name,
          attrs->target ? " -> " : "",
          attrs->target ? attrs->target : "",
          flags & FORMAT_ATTRS ? " " : "",
          flags & FORMAT_ATTRS ? bits : "");
  return formatted;
}

uint32_t sftp_normalize_ownergroup(struct allocator *a, struct sftpattr *attrs) {
  uint32_t rc = SSH_FX_OK;

  switch(attrs->valid & (SSH_FILEXFER_ATTR_UIDGID
                         |SSH_FILEXFER_ATTR_OWNERGROUP)) {
  case SSH_FILEXFER_ATTR_UIDGID:
    if((attrs->owner = sftp_uid2name(a, attrs->uid))
       && (attrs->group = sftp_gid2name(a, attrs->gid)))
      attrs->valid |= SSH_FILEXFER_ATTR_OWNERGROUP;
    break;
  case SSH_FILEXFER_ATTR_OWNERGROUP:
    if((attrs->uid = sftp_name2uid(attrs->owner)) == (uid_t)-1)
      rc = SSH_FX_OWNER_INVALID;
    if((attrs->gid = sftp_name2gid(attrs->group)) == (gid_t)-1)
      rc = SSH_FX_GROUP_INVALID;
    if(rc == SSH_FX_OK)
      attrs->valid |= SSH_FILEXFER_ATTR_UIDGID;
    break;
  }
  return rc;
}

struct sftp_set_status_callbacks {
  int (*do_truncate)(const void *what, off_t size);
  int (*do_chown)(const void *what, uid_t uid, gid_t gid);
  int (*do_chmod)(const void *what, mode_t mode);
  int (*do_stat)(const void *what, struct stat *sb);
  int (*do_utimes)(const void *what, struct timeval *tv);
};

/* Horrendous ugliness for SETSTAT/FSETSTAT */
#if HAVE_STAT_TIMESPEC
#define SET_STATUS_NANOSEC do {                                         \
    times[0].tv_usec = ((attrs.valid & SSH_FILEXFER_ATTR_ACCESSTIME)    \
                        ? (long)attrs.atime.nanoseconds                 \
                        : current.st_atimespec.tv_nsec) / 1000;         \
    times[1].tv_usec = ((attrs.valid & SSH_FILEXFER_ATTR_MODIFYTIME)    \
                        ? (long)attrs.mtime.nanoseconds                 \
                        : current.st_mtimespec.tv_nsec) / 1000;         \
} while(0)
#else
#define SET_STATUS_NANOSEC ((void)0)
#endif

static uint32_t do_sftp_set_status(struct allocator *a,
                              const void *what,
                              const struct sftpattr *attrsp,
                              const struct sftp_set_status_callbacks *cb,
                              const char **whyp) {
  struct timeval times[2];
  struct stat current;
  struct sftpattr attrs = *attrsp;
  const char *why;

  if(!whyp)
    whyp = &why;
  *whyp = 0;
  if((attrs.valid & (SSH_FILEXFER_ATTR_SIZE|SSH_FILEXFER_ATTR_ALLOCATION_SIZE))
     == (SSH_FILEXFER_ATTR_SIZE|SSH_FILEXFER_ATTR_ALLOCATION_SIZE)
     && attrs.allocation_size < attrs.size) {
    /* This is a MUST, e.g. draft -13 s7.4.  We honor it even though we don't
     * honor allocation-size. */
    *whyp = "size exceeds allocation-size";
    return SSH_FX_INVALID_PARAMETER;
  }
  if(attrs.valid & (SSH_FILEXFER_ATTR_LINK_COUNT
                    |SSH_FILEXFER_ATTR_TEXT_HINT))
    /* Client has violated a MUST NOT */
    return SSH_FX_INVALID_PARAMETER;
  if((attrs.valid & (SSH_FILEXFER_ATTR_SIZE
                     |SSH_FILEXFER_ATTR_PERMISSIONS
                     |SSH_FILEXFER_ATTR_ACCESSTIME
                     |SSH_FILEXFER_ATTR_MODIFYTIME
                     |SSH_FILEXFER_ATTR_UIDGID
                     |SSH_FILEXFER_ATTR_OWNERGROUP
                     |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
                     |SSH_FILEXFER_ATTR_ALLOCATION_SIZE)) != attrs.valid) {
    /* SHOULD not change any attributes if we cannot perform as required.  We
     * have a SHOULD which allows us to ignore allocation-size. */
    *whyp = "unsupported flags";
    return SSH_FX_OP_UNSUPPORTED;
  }
  if(attrs.valid & SSH_FILEXFER_ATTR_SIZE) {
    D(("...truncate to %"PRIu64, attrs.size));
    if(cb->do_truncate(what, attrs.size) < 0) {
      *whyp = "truncate";
      return HANDLER_ERRNO;
    }
  }
  sftp_normalize_ownergroup(a, &attrs);
  if(attrs.valid & SSH_FILEXFER_ATTR_UIDGID) {
    D(("...chown to %"PRId32"/%"PRId32, attrs.uid, attrs.gid));
    if(cb->do_chown(what, attrs.uid, attrs.gid) < 0) {
      *whyp = "chown";
      return HANDLER_ERRNO;
    }
  }
  if(attrs.valid & SSH_FILEXFER_ATTR_PERMISSIONS) {
    const mode_t mode = attrs.permissions & 07777;
    D(("...chmod to %#o", (unsigned)mode));
    if(cb->do_chmod(what, mode) < 0) {
      *whyp = "chmod";
      return HANDLER_ERRNO;
    }
  }
  if(attrs.valid & (SSH_FILEXFER_ATTR_ACCESSTIME
                     |SSH_FILEXFER_ATTR_MODIFYTIME)) {
    if(cb->do_stat(what, &current) < 0) {
      D(("cannot stat"));
      *whyp = "stat";
      return HANDLER_ERRNO;
    }
    memset(times, 0, sizeof times);
    times[0].tv_sec = ((attrs.valid & SSH_FILEXFER_ATTR_ACCESSTIME)
                       ? (time_t)attrs.atime.seconds
                       : current.st_atime);
    times[1].tv_sec = ((attrs.valid & SSH_FILEXFER_ATTR_MODIFYTIME)
                       ? (time_t)attrs.mtime.seconds
                       : current.st_mtime);
#if HAVE_STAT_TIMESPEC
    if(attrs.valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {
      times[0].tv_usec = ((attrs.valid & SSH_FILEXFER_ATTR_ACCESSTIME)
                          ? (long)attrs.atime.nanoseconds
                          : current.st_atimespec.tv_nsec) / 1000;
      times[1].tv_usec = ((attrs.valid & SSH_FILEXFER_ATTR_MODIFYTIME)
                          ? (long)attrs.mtime.nanoseconds
                          : current.st_mtimespec.tv_nsec) / 1000;
    }
#endif
    D(("...utimes to atime %lu.%06lu mtime %lu.%06lu",
       (unsigned long)times[0].tv_sec,
       (unsigned long)times[0].tv_usec,
       (unsigned long)times[1].tv_sec,
       (unsigned long)times[1].tv_usec));
    if(cb->do_utimes(what, times) < 0) {
      *whyp = "utimes";
      return HANDLER_ERRNO;
    }
  }
  return SSH_FX_OK;
}

static int name_truncate(const void *what, off_t size) {
  return truncate(what, size);
}

static int name_chown(const void *what, uid_t uid, gid_t gid) {
  return chown(what, uid, gid);
}

static int name_chmod(const void *what, mode_t mode) {
  return chmod(what, mode);
}

static int name_lstat(const void *what, struct stat *sb) {
  return lstat(what, sb);
}

static int name_utimes(const void *what, struct timeval *tv) {
  return utimes(what, tv);
}

static const struct sftp_set_status_callbacks name_callbacks = {
  name_truncate,
  name_chown,
  name_chmod,
  name_lstat,
  name_utimes
};

uint32_t sftp_set_status(struct allocator *a,
                    const char *path,
                    const struct sftpattr *attrsp,
                    const char **whyp) {
  return do_sftp_set_status(a, path, attrsp, &name_callbacks, whyp);
}

static int fd_truncate(const void *what, off_t size) {
  return ftruncate(*(const int *)what, size);
}

static int fd_chown(const void *what, uid_t uid, gid_t gid) {
  return fchown(*(const int *)what, uid, gid);
}

static int fd_chmod(const void *what, mode_t mode) {
  return fchmod(*(const int *)what, mode);
}

static int fd_stat(const void *what, struct stat *sb) {
  return fstat(*(const int *)what, sb);
}

static int fd_utimes(const void *what, struct timeval *tv) {
  return futimes(*(const int *)what, tv);
}

static const struct sftp_set_status_callbacks fd_callbacks = {
  fd_truncate,
  fd_chown,
  fd_chmod,
  fd_stat,
  fd_utimes
};

uint32_t sftp_set_fstatus(struct allocator *a,
                     int fd,
                     const struct sftpattr *attrsp,
                     const char **whyp) {
  return do_sftp_set_status(a, &fd, attrsp, &fd_callbacks, whyp);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
