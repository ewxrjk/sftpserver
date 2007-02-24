#include "sftpserver.h"
#include "sftp.h"
#include "send.h"
#include "types.h"
#include "globals.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

void generic_status(struct sftpjob *job, 
		    uint32_t status,
		    uint32_t original_status,
		    const char *msg) {
  if(status == (uint32_t)-1) {
    /* Bodge to allow us to treat -1 as a magical status meaning 'consult
     * errno'.  This goes back via the protocol-specific status callback, so
     * statuses out of range for the current protocol version get properly
     * laundered. */
    send_errno_status(job);
    return;
  }
  if(!msg) {
    switch(original_status) {
    case SSH_FX_OK: msg = "OK"; break;
    case SSH_FX_EOF: msg = "end of file"; break;
    case SSH_FX_NO_SUCH_FILE: msg = "file does not exist"; break;
    case SSH_FX_PERMISSION_DENIED: msg = "permission denied"; break;
    case SSH_FX_FAILURE: msg = "operation failed"; break;
    case SSH_FX_BAD_MESSAGE: msg = "badly encoded SFTP packet"; break;
    case SSH_FX_NO_CONNECTION: assert(!"cannot happen");
    case SSH_FX_CONNECTION_LOST: assert(!"cannot happen");
    case SSH_FX_OP_UNSUPPORTED: msg = "operation not supported"; break;
    case SSH_FX_INVALID_HANDLE: msg = "invalid handle"; break;
    case SSH_FX_NO_SUCH_PATH: msg = "path does not exist or is invalid"; break;
    case SSH_FX_FILE_ALREADY_EXISTS: msg = "file already exists"; break;
    case SSH_FX_WRITE_PROTECT: msg = "file is on read-only medium"; break;
    case SSH_FX_NO_MEDIA: msg = "no medium in drive"; break;
    case SSH_FX_NO_SPACE_ON_FILESYSTEM: msg = "no space on filesystem"; break;
    case SSH_FX_QUOTA_EXCEEDED: msg = "quota exceeded"; break;
    case SSH_FX_UNKNOWN_PRINCIPAL: assert(!"cannot happen"); /* needs ESD */
    case SSH_FX_LOCK_CONFLICT: msg = "file is locked"; break;
    case SSH_FX_DIR_NOT_EMPTY: msg = "directory is not empty"; break;
    case SSH_FX_NOT_A_DIRECTORY: msg = "file is not a directory"; break;
    case SSH_FX_INVALID_FILENAME: msg = "invalid filename"; break;
    case SSH_FX_LINK_LOOP: msg = "too many symbolic links"; break;
    case SSH_FX_CANNOT_DELETE: msg = "file cannot be deleted"; break;
    case SSH_FX_INVALID_PARAMETER: msg = "invalid parameter"; break;
    case SSH_FX_FILE_IS_A_DIRECTORY: msg = "file is a directory"; break;
    case SSH_FX_BYTE_RANGE_LOCK_CONFLICT: msg = "byte range is locked"; break;
    case SSH_FX_BYTE_RANGE_LOCK_REFUSED: msg = "cannot lock byte range"; break;
    case SSH_FX_DELETE_PENDING: msg = "file deletion pending"; break;
    case SSH_FX_FILE_CORRUPT: msg = "file is corrupt"; break;
    case SSH_FX_OWNER_INVALID: msg = "invalid owner"; break;
    case SSH_FX_GROUP_INVALID: msg = "invalid group"; break;
    case SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK: msg = "no such lock"; break;
    default: msg = "unknown status"; break;
    }
  }
  send_begin(job);
  send_uint8(job, SSH_FXP_STATUS);
  send_uint32(job, job->id);
  send_uint32(job, status);
  send_string(job, msg);
  send_string(job, "en");               /* we are not I18N'd yet */
  send_end(job);
}

static const struct {
  int errno_value;
  uint32_t status_value;
} errnotab[] = {
  { 0, SSH_FX_OK },
  { EPERM, SSH_FX_PERMISSION_DENIED },
  { EACCES, SSH_FX_PERMISSION_DENIED },
  { ENOENT, SSH_FX_NO_SUCH_FILE },
  { EIO, SSH_FX_FILE_CORRUPT },
  { ENOSPC, SSH_FX_NO_SPACE_ON_FILESYSTEM },
  { ENOTDIR, SSH_FX_NOT_A_DIRECTORY },
  { EISDIR, SSH_FX_FILE_IS_A_DIRECTORY },
  { EEXIST, SSH_FX_FILE_ALREADY_EXISTS },
  { EROFS, SSH_FX_WRITE_PROTECT },
  { ELOOP, SSH_FX_LINK_LOOP },
  { ENAMETOOLONG, SSH_FX_INVALID_FILENAME },
  { ENOTEMPTY, SSH_FX_DIR_NOT_EMPTY },
  { EDQUOT, SSH_FX_QUOTA_EXCEEDED },
  { -1, SSH_FX_FAILURE },
};

void send_errno_status(struct sftpjob *job) {
  int n;
  const int errno_value = errno;

  for(n = 0; 
      errnotab[n].errno_value != errno_value && errnotab[n].errno_value != -1;
      ++n)
    ;
  protocol->status(job, errnotab[n].status_value, strerror(errno_value));
}

void send_ok(struct sftpjob *job) {
  protocol->status(job, SSH_FX_OK, 0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
