#include "sftpserver.h"
#include "types.h"
#include "users.h"
#include "sftp.h"
#include <sys/stat.h>
#include <string.h>

void stat_to_attrs(struct allocator *a,
		   const struct stat *sb, struct sftpattr *attrs) {
  memset(attrs, 0, sizeof *attrs);
  attrs->valid = (SSH_FILEXFER_ATTR_SIZE
		  |SSH_FILEXFER_ATTR_PERMISSIONS
		  |SSH_FILEXFER_ATTR_ACCESSTIME
		  |SSH_FILEXFER_ATTR_CREATETIME
		  |SSH_FILEXFER_ATTR_MODIFYTIME
		  |SSH_FILEXFER_ATTR_OWNERGROUP
		  |SSH_FILEXFER_ATTR_ALLOCATION_SIZE
		  |SSH_FILEXFER_ATTR_LINK_COUNT
		  |SSH_FILEXFER_ATTR_CTIME);
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
  attrs->allocation_size = sb->st_blksize;
  attrs->owner = uid2name(a, sb->st_uid);
  attrs->uid = sb->st_uid;
  attrs->group = gid2name(a, sb->st_gid);
  attrs->gid = sb->st_gid;
  attrs->permissions = sb->st_mode;
  attrs->atime.seconds = sb->st_atime;
  attrs->mtime.seconds = sb->st_mtime;
  attrs->ctime.seconds = sb->st_ctime;
#if HAVE_STAT_TIMESPEC
  attrs->atime.nanoseconds = 1000 * sb->st_atimespec.tv_nsec;
  attrs->mtime.nanoseconds = 1000 * sb->st_mtimespec.tv_nsec;
  attrs->ctime.nanoseconds = 1000 * sb->st_ctimespec.tv_nsec;
  attrs->valid |= SSH_FILEXFER_ATTR_SUBSECOND_TIMES;
#endif
  attrs->link_count = sb->st_nlink;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
