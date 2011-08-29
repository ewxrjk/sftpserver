/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2011 Richard Kettlewell
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

/** @file sftp.h @brief SFTP definitions
 *
 * Specifications:
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-02.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-04.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-05.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-13.txt
 *
 * See also:
 * - @ref type
 * - @ref valid_attribute_flags
 * - @ref attrib_bits
 * - @ref text_hint
 * - @ref request_flags
 * - @ref rename_flags
 * - @ref realpath_control
 * - @ref open_pflags
 * - @ref status
 *
 */

#ifndef SFTP_H
#define SFTP_H

/** @defgroup type SFTP 'type' definitions
 * @{
 */

/** @brief Protocol initialization */
#define SSH_FXP_INIT                1

/** @brief Server version */
#define SSH_FXP_VERSION             2

/** @brief Open file */
#define SSH_FXP_OPEN                3

/** @brief Close handle */
#define SSH_FXP_CLOSE               4

/** @brief Read bytes */
#define SSH_FXP_READ                5

/** @brief Write bytes */
#define SSH_FXP_WRITE               6

/** @brief Get file attributes by name
 *
 * Do not follow symlinks.
 */
#define SSH_FXP_LSTAT               7

/** @brief Get file attributes by handle */
#define SSH_FXP_FSTAT               8

/** @brief Set file attributes by name */
#define SSH_FXP_SETSTAT             9

/** @brief Set file attributes by handle */
#define SSH_FXP_FSETSTAT           10   /* 0x0A */

/** @brief Open directory */
#define SSH_FXP_OPENDIR            11   /* 0x0B */

/** @brief Read from directory */
#define SSH_FXP_READDIR            12   /* 0x0C */

/** @brief Remove file */
#define SSH_FXP_REMOVE             13   /* 0x0D */

/** @brief Create directory */
#define SSH_FXP_MKDIR              14   /* 0x0E */

/** @brief Remove directory */
#define SSH_FXP_RMDIR              15   /* 0x0F */

/** @brief Canonicalize path */
#define SSH_FXP_REALPATH           16   /* 0x10 */

/** @brief Get file attributes by name
 *
 * Do follow symlinks.
 */
#define SSH_FXP_STAT               17   /* 0x11 */

/** @brief Rename file */
#define SSH_FXP_RENAME             18   /* 0x12 */

/** @brief Read symbolic link */
#define SSH_FXP_READLINK           19   /* 0x13 */

/** @brief Create symbolic link */
#define SSH_FXP_SYMLINK            20   /* 0x14 */

/** @brief Create hard link */
#define SSH_FXP_LINK               21   /* 0x15 */

/** @brief Lock byte range */
#define SSH_FXP_BLOCK              22   /* 0x16 */

/** @brief Unlock byte range */
#define SSH_FXP_UNBLOCK            23   /* 0x17 */

/** @brief Response with a status */
#define SSH_FXP_STATUS            101   /* 0x65 */

/** @brief Response with a handle */
#define SSH_FXP_HANDLE            102   /* 0x66 */

/** @brief Response with data bytes */
#define SSH_FXP_DATA              103   /* 0x67 */

/** @brief Response with a name */
#define SSH_FXP_NAME              104   /* 0x68 */

/** @brief Response with attributes */
#define SSH_FXP_ATTRS             105   /* 0x69 */

/** @brief Execute an extended command */
#define SSH_FXP_EXTENDED          200   /* 0xC8 */

/** @brief Extended response format */
#define SSH_FXP_EXTENDED_REPLY    201   /* 0xC9 */

/** @} */

#define SSH_ACL_CAP_ALLOW                       0x00000001
#define SSH_ACL_CAP_DENY                        0x00000002
#define SSH_ACL_CAP_AUDIT                       0x00000004
#define SSH_ACL_CAP_ALARM                       0x00000008
#define SSH_ACL_CAP_INHERIT_ACCESS              0x00000010
#define SSH_ACL_CAP_INHERIT_AUDIT_ALARM         0x00000020

/** @defgroup valid_attribute_flags SFTP 'valid-attribute-flags' definitions
 * @{
 */

/** @brief @c size field is present */
#define SSH_FILEXFER_ATTR_SIZE              0x00000001

/** @brief @c uid and @c gid fiels are present
 *
 * This is for v3 only.
 */
#define SSH_FILEXFER_ATTR_UIDGID            0x00000002

/** @brief @c permissions field is present */
#define SSH_FILEXFER_ATTR_PERMISSIONS       0x00000004

/** @brief @c atime field is present
 *
 * This is for v4 and later.
 */
#define SSH_FILEXFER_ATTR_ACCESSTIME        0x00000008

/** @brief @c atime and @c mtime fiels are present
 *
 * This is for v3 only.
 */
#define SSH_FILEXFER_ACMODTIME              0x00000008

/** @brief @c createtime field is present */
#define SSH_FILEXFER_ATTR_CREATETIME        0x00000010

/** @brief @c mtime field is present */
#define SSH_FILEXFER_ATTR_MODIFYTIME        0x00000020

/** @brief @c ACL field is present */
#define SSH_FILEXFER_ATTR_ACL               0x00000040

/** @brief @c owner and @c group fields are present */
#define SSH_FILEXFER_ATTR_OWNERGROUP        0x00000080

/** @brief Times include subsecond fields */
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES   0x00000100

/** @brief @c attrib-bits and @c attrib-bits-valid fields are present */
#define SSH_FILEXFER_ATTR_BITS              0x00000200

/** @brief @c allocaiton-size field is present */
#define SSH_FILEXFER_ATTR_ALLOCATION_SIZE   0x00000400

/** @brief @c text-hint field is present */
#define SSH_FILEXFER_ATTR_TEXT_HINT         0x00000800

/** @brief @c mime-type field is present */
#define SSH_FILEXFER_ATTR_MIME_TYPE         0x00001000

/** @brief @c link-count field is present */
#define SSH_FILEXFER_ATTR_LINK_COUNT        0x00002000

/** @brief @c untranslated-name field is present */
#define SSH_FILEXFER_ATTR_UNTRANSLATED_NAME 0x00004000

/** @brief @c time field is present */
#define SSH_FILEXFER_ATTR_CTIME             0x00008000

/** @brief Extended attributes present */
#define SSH_FILEXFER_ATTR_EXTENDED          0x80000000

/** @} */

/** @defgroup file_type SFTP file types
 * @{
 */

/** @brief Regular file */
#define SSH_FILEXFER_TYPE_REGULAR          1

/** @brief Directory */
#define SSH_FILEXFER_TYPE_DIRECTORY        2

/** @brief Symbolic link */
#define SSH_FILEXFER_TYPE_SYMLINK          3

/** @brief Special file
 *
 * This is for files where the type is known but cannot be exprssed in SFTP. */
#define SSH_FILEXFER_TYPE_SPECIAL          4

/** @brief Unknown file type */
#define SSH_FILEXFER_TYPE_UNKNOWN          5

/** @brief Socket */
#define SSH_FILEXFER_TYPE_SOCKET           6

/** @brief Character device */
#define SSH_FILEXFER_TYPE_CHAR_DEVICE      7

/** @brief Block device */
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE     8

/** @brief Named pipe */
#define SSH_FILEXFER_TYPE_FIFO             9

/** @} */

#define SFX_ACL_CONTROL_INCLUDED        0x00000001
#define SFX_ACL_CONTROL_PRESENT         0x00000002
#define SFX_ACL_CONTROL_INHERITED       0x00000004
#define SFX_ACL_AUDIT_ALARM_INCLUDED    0x00000010

#define ACE4_ACCESS_ALLOWED_ACE_TYPE 0x00000000
#define ACE4_ACCESS_DENIED_ACE_TYPE  0x00000001
#define ACE4_SYSTEM_AUDIT_ACE_TYPE   0x00000002
#define ACE4_SYSTEM_ALARM_ACE_TYPE   0x00000003

#define ACE4_FILE_INHERIT_ACE           0x00000001
#define ACE4_DIRECTORY_INHERIT_ACE      0x00000002
#define ACE4_NO_PROPAGATE_INHERIT_ACE   0x00000004
#define ACE4_INHERIT_ONLY_ACE           0x00000008
#define ACE4_SUCCESSFUL_ACCESS_ACE_FLAG 0x00000010
#define ACE4_FAILED_ACCESS_ACE_FLAG     0x00000020
#define ACE4_IDENTIFIER_GROUP           0x00000040

#define ACE4_READ_DATA         0x00000001
#define ACE4_LIST_DIRECTORY    0x00000001
#define ACE4_WRITE_DATA        0x00000002
#define ACE4_ADD_FILE          0x00000002
#define ACE4_APPEND_DATA       0x00000004
#define ACE4_ADD_SUBDIRECTORY  0x00000004
#define ACE4_READ_NAMED_ATTRS  0x00000008
#define ACE4_WRITE_NAMED_ATTRS 0x00000010
#define ACE4_EXECUTE           0x00000020
#define ACE4_DELETE_CHILD      0x00000040
#define ACE4_READ_ATTRIBUTES   0x00000080
#define ACE4_WRITE_ATTRIBUTES  0x00000100
#define ACE4_DELETE            0x00010000
#define ACE4_READ_ACL          0x00020000
#define ACE4_WRITE_ACL         0x00040000
#define ACE4_WRITE_OWNER       0x00080000
#define ACE4_SYNCHRONIZE       0x00100000

/** @defgroup attrib_bits SFTP 'attrib-bits' definitions
 * @{
 */

/** @brief File is read-only (advisory) */
#define SSH_FILEXFER_ATTR_FLAGS_READONLY         0x00000001

/** @brief File is part of the OS */
#define SSH_FILEXFER_ATTR_FLAGS_SYSTEM           0x00000002

/** @brief File is hidden */
#define SSH_FILEXFER_ATTR_FLAGS_HIDDEN           0x00000004

/** @brief Filenames in this directory are case-insensitive */
#define SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE 0x00000008

/** @brief File should be archived */
#define SSH_FILEXFER_ATTR_FLAGS_ARCHIVE          0x00000010

/** @brief File is stored on encrypted media */
#define SSH_FILEXFER_ATTR_FLAGS_ENCRYPTED        0x00000020

/** @brief File is stored on compressed media */
#define SSH_FILEXFER_ATTR_FLAGS_COMPRESSED       0x00000040

/** @brief File is sparse */
#define SSH_FILEXFER_ATTR_FLAGS_SPARSE           0x00000080

/** @brief File is append-only */
#define SSH_FILEXFER_ATTR_FLAGS_APPEND_ONLY      0x00000100

/** @brief File cannot be delete, renamed, linked to or written */
#define SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE        0x00000200

/** @brief Modifications to file are synchronous */
#define SSH_FILEXFER_ATTR_FLAGS_SYNC             0x00000400

/** @brief Filename could not be converted to UTF-8 */
#define SSH_FILEXFER_ATTR_FLAGS_TRANSLATION_ERR  0x00000800

/** @} */

/** @defgroup text_hint SFTP 'text-hint' definitions
 * @{
 */

/** @brief Server knows file is a text file */
#define SSH_FILEXFER_ATTR_KNOWN_TEXT        0x00

/** @brief Server believes file is a text file */
#define SSH_FILEXFER_ATTR_GUESSED_TEXT      0x01

/** @brief Server knows file is a binary file */
#define SSH_FILEXFER_ATTR_KNOWN_BINARY      0x02

/** @brief Server believes file is a binary file */
#define SSH_FILEXFER_ATTR_GUESSED_BINARY    0x03

/** @} */

/** @defgroup request_flags SFTP operation 'flags' definitions
 * @{
 */

/** @brief Mask of file open bits */
#define SSH_FXF_ACCESS_DISPOSITION       0x00000007

/** @brief Must create a new file */
#define SSH_FXF_CREATE_NEW           0x00000000

/** @brief Create new file or truncate existing one */
#define SSH_FXF_CREATE_TRUNCATE      0x00000001

/** @brief File must already exist */
#define SSH_FXF_OPEN_EXISTING        0x00000002

/** @brief Open existing file or create new one */
#define SSH_FXF_OPEN_OR_CREATE       0x00000003

/** @brief Truncate an existing file */
#define SSH_FXF_TRUNCATE_EXISTING    0x00000004

/** @brief Lossy append only */
#define SSH_FXF_APPEND_DATA              0x00000008

/** @brief Append only */
#define SSH_FXF_APPEND_DATA_ATOMIC       0x00000010

/** @brief Convert newlines */
#define SSH_FXF_TEXT_MODE                0x00000020

/** @brief Exclusive read access */
#define SSH_FXF_BLOCK_READ               0x00000040

/** @brief Exclusive write access */
#define SSH_FXF_BLOCK_WRITE              0x00000080

/** @brief Exclusive delete access */
#define SSH_FXF_BLOCK_DELETE             0x00000100

/** @brief Other @c SSH_FXF_BLOCK_... bits are advisory */
#define SSH_FXF_BLOCK_ADVISORY           0x00000200

/** @brief Do not follow symlinks */
#define SSH_FXF_NOFOLLOW                 0x00000400

/** @brief Delete when last handle closed */
#define SSH_FXF_DELETE_ON_CLOSE          0x00000800

/** @brief Enable audit/alarm privileges */
#define SSH_FXF_ACCESS_AUDIT_ALARM_INFO  0x00001000

/** @brief Enable backup privileges */
#define SSH_FXF_ACCESS_BACKUP            0x00002000

/** @brief Read or write backup stream */
#define SSH_FXF_BACKUP_STREAM            0x00004000

/** @brief Enable owner privileges */
#define SSH_FXF_OVERRIDE_OWNER           0x00008000

/** @} */

/** @defgroup rename_flags SFTP rename flag definitions
 * @{
 */

/** @brief Overwrite target if it exists */
#define SSH_FXF_RENAME_OVERWRITE  0x00000001

/** @brief Ensure target name always exists and refers to original or new file */
#define SSH_FXF_RENAME_ATOMIC     0x00000002

/** @brief Remove requirements on server */
#define SSH_FXF_RENAME_NATIVE     0x00000004

/** @} */

/** @defgroup realpath_control SFTP realpath control byte definitions
 * @{
 */

/** @brief Don't check for existence or accessibility, don't resolve links */
#define SSH_FXP_REALPATH_NO_CHECK    0x00000001

/** @brief Get file attribtues if it exists */
#define SSH_FXP_REALPATH_STAT_IF     0x00000002

/** @brief Get file attributes, fail if unavailable */
#define SSH_FXP_REALPATH_STAT_ALWAYS 0x00000003

/** @} */

/** @defgroup open_pflags SFTP open/create 'pflags' definitions
 * @{
 */

/** @brief Open file for reading */
#define SSH_FXF_READ            0x00000001

/** @brief Open file for writing */
#define SSH_FXF_WRITE           0x00000002

/** @brief Append to end of file */
#define SSH_FXF_APPEND          0x00000004

/** @brief Create file if it does not exist */
#define SSH_FXF_CREAT           0x00000008

/** @brief Truncate file if it already exists */
#define SSH_FXF_TRUNC           0x00000010

/** @brief File must not exist */
#define SSH_FXF_EXCL            0x00000020

/** @brief Convert newlines */
#define SSH_FXF_TEXT            0x00000040

/** @} */

/** @defgroup status SFTP error/status codes
 * @{
 */

/** @brief Success */
#define SSH_FX_OK                            0

/** @brief End of file */
#define SSH_FX_EOF                           1

/** @brief File does not exist */
#define SSH_FX_NO_SUCH_FILE                  2

/** @brief Insufficient permission */
#define SSH_FX_PERMISSION_DENIED             3

/** @brief Unknown error */
#define SSH_FX_FAILURE                       4

/** @brief Badly formatted packet */
#define SSH_FX_BAD_MESSAGE                   5

/** @brief No connection to server */
#define SSH_FX_NO_CONNECTION                 6

/** @brief Connection to server lost */
#define SSH_FX_CONNECTION_LOST               7

/** @brief Operation not supported by server */
#define SSH_FX_OP_UNSUPPORTED                8

/** @brief Invalid handle value */
#define SSH_FX_INVALID_HANDLE                9  /*      v4+ */

/** @brief File path does not exist or is invalid */
#define SSH_FX_NO_SUCH_PATH                  10 /* 0x0A */

/** @brief File already exists */
#define SSH_FX_FILE_ALREADY_EXISTS           11 /* 0x0B */

/** @brief File is on read-only or write-protected media */
#define SSH_FX_WRITE_PROTECT                 12 /* 0x0C */

/** @brief No medium in drive */
#define SSH_FX_NO_MEDIA                      13 /* 0x0D */

/** @brief Insufficient free space */
#define SSH_FX_NO_SPACE_ON_FILESYSTEM        14 /* 0x0E v5+ */

/** @brief User's storage quota would be exceeded */
#define SSH_FX_QUOTA_EXCEEDED                15 /* 0x0F */

/** @brief Unknown principal in ACL */
#define SSH_FX_UNKNOWN_PRINCIPAL             16 /* 0x10 */

/** @brief File is locked by another process */
#define SSH_FX_LOCK_CONFLICT                 17 /* 0x11 */

/** @brief Directory is not empty */
#define SSH_FX_DIR_NOT_EMPTY                 18 /* 0x12 v6+ */

/** @brief File is not a director */
#define SSH_FX_NOT_A_DIRECTORY               19 /* 0x13 */

/** @brief Filename is invalid */
#define SSH_FX_INVALID_FILENAME              20 /* 0x14 */

/** @brief Too many symbolic links or link found by @ref SSH_FXF_NOFOLLOW */
#define SSH_FX_LINK_LOOP                     21 /* 0x15 */

/** @brief File cannot be deleted */
#define SSH_FX_CANNOT_DELETE                 22 /* 0x16 */

/** @brief Parameter out of range or conflicting */
#define SSH_FX_INVALID_PARAMETER             23 /* 0x17 */

/** @brief File is a directory */
#define SSH_FX_FILE_IS_A_DIRECTORY           24 /* 0x18 */

/** @brief Mandatory lock conflict when reading or writing */
#define SSH_FX_BYTE_RANGE_LOCK_CONFLICT      25 /* 0x19 */

/** @brief Lock request refused */
#define SSH_FX_BYTE_RANGE_LOCK_REFUSED       26 /* 0x1A */

/** @brief Delete operation pending on faile */
#define SSH_FX_DELETE_PENDING                27 /* 0x1B */

/** @brief File is corrupt */
#define SSH_FX_FILE_CORRUPT                  28 /* 0x1C */

/** @brief Principal cannot become file owner */
#define SSH_FX_OWNER_INVALID                 29 /* 0x1D */

/** @brief Principal cannot become file group */
#define SSH_FX_GROUP_INVALID                 30 /* 0x1E */

/** @brief Specified lock range has not been granted */
#define SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK   31 /* 0x1F */

/** @} */

/** @mainpage Green End SFTP Server
 *
 * This is the internal documentation for the Green End SFTP server.
 * It is intended for developers of the server, not users.
 *
 * @section structure Server Structure
 *
 * @subsection concurrency Concurrency
 *
 * The server is designed to be able to execute
 * multiple requests concurrently.  To this end, sftp_service() reads
 * requests from its input and (once initialization is complete) adds
 * them to a @ref queue in the form of an @ref sftpjob.  The queue is
 * serviced by a pool of worker threads, which are responsible for
 * executing the requests and sending responses.
 *
 * Each thread has a separate memory @ref allocator.  All memory
 * allocated to it is released after each request is processed.
 *
 * The SFTP specification makes certain demands about the order in
 * which responses appear.  These are implemented in @ref serialize.c.
 *
 * @subsection versions Protocol Versions
 *
 * The server supports the four proposed versions of the protocol.
 * Each protocol version has a separate table of request
 * implementations and other details, in an @ref sftpprotocol object.
 * Much, though not all, of the implementation is shared between them.
 *
 * The convention for request implementation naming can be observed in
 * @ref sftpserver.h.  Each function is named @c
 * sftp_vVERSIONS_REQUEST, where @c VERSIONS is replaced with by the
 * list of versions supported or by @c any if the implementation is
 * completely generic.
 *
 * @subsection Attributes
 *
 * Although attribute specifications vary between the protocol
 * versions they are represented inside the server in a common @ref
 * sftpattr structure. Each protocol version has a @c parseattrs and a
 * @c sendattrs callback which convert between wire and internal
 * format.
 *
 * @section specs Specifications
 *
 * - http://www.ietf.org/rfc/rfc4251.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-02.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-04.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-05.txt
 * - http://tools.ietf.org/wg/secsh/draft-ietf-secsh-filexfer/draft-ietf-secsh-filexfer-13.txt
 *
 * @section links Links
 *
 * - http://www.greenend.org.uk/rjk/sftpserver/
 * - https://github.com/ewxrjk/sftpserver
 * - http://www.greenend.org.uk/rjk/2007/sftpversions.html
 */

#endif /* SFTP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
#define End:
*/
