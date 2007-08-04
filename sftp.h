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

#ifndef SFTP_H
#define SFTP_H

#define SSH_FXP_INIT                1
#define SSH_FXP_VERSION             2
#define SSH_FXP_OPEN                3
#define SSH_FXP_CLOSE               4
#define SSH_FXP_READ                5
#define SSH_FXP_WRITE               6
#define SSH_FXP_LSTAT               7
#define SSH_FXP_FSTAT               8
#define SSH_FXP_SETSTAT             9
#define SSH_FXP_FSETSTAT           10   /* 0x0A */
#define SSH_FXP_OPENDIR            11   /* 0x0B */
#define SSH_FXP_READDIR            12   /* 0x0C */
#define SSH_FXP_REMOVE             13   /* 0x0D */
#define SSH_FXP_MKDIR              14   /* 0x0E */
#define SSH_FXP_RMDIR              15   /* 0x0F */
#define SSH_FXP_REALPATH           16   /* 0x10 */
#define SSH_FXP_STAT               17   /* 0x11 */
#define SSH_FXP_RENAME             18   /* 0x12 */
#define SSH_FXP_READLINK           19   /* 0x13 */
#define SSH_FXP_SYMLINK            20   /* 0x14 */
#define SSH_FXP_LINK               21   /* 0x15 */
#define SSH_FXP_BLOCK              22   /* 0x16 */
#define SSH_FXP_UNBLOCK            23   /* 0x17 */
#define SSH_FXP_STATUS            101   /* 0x65 */
#define SSH_FXP_HANDLE            102   /* 0x66 */
#define SSH_FXP_DATA              103   /* 0x67 */
#define SSH_FXP_NAME              104   /* 0x68 */
#define SSH_FXP_ATTRS             105   /* 0x69 */
#define SSH_FXP_EXTENDED          200   /* 0xC8 */
#define SSH_FXP_EXTENDED_REPLY    201   /* 0xC9 */

#define SSH_ACL_CAP_ALLOW                       0x00000001
#define SSH_ACL_CAP_DENY                        0x00000002
#define SSH_ACL_CAP_AUDIT                       0x00000004
#define SSH_ACL_CAP_ALARM                       0x00000008
#define SSH_ACL_CAP_INHERIT_ACCESS              0x00000010
#define SSH_ACL_CAP_INHERIT_AUDIT_ALARM         0x00000020

#define SSH_FILEXFER_ATTR_SIZE              0x00000001
#define SSH_FILEXFER_ATTR_UIDGID            0x00000002
#define SSH_FILEXFER_ATTR_PERMISSIONS       0x00000004
#define SSH_FILEXFER_ATTR_ACCESSTIME        0x00000008
#define SSH_FILEXFER_ACMODTIME              0x00000008 /* v3 only */
#define SSH_FILEXFER_ATTR_CREATETIME        0x00000010
#define SSH_FILEXFER_ATTR_MODIFYTIME        0x00000020
#define SSH_FILEXFER_ATTR_ACL               0x00000040
#define SSH_FILEXFER_ATTR_OWNERGROUP        0x00000080
#define SSH_FILEXFER_ATTR_SUBSECOND_TIMES   0x00000100
#define SSH_FILEXFER_ATTR_BITS              0x00000200
#define SSH_FILEXFER_ATTR_ALLOCATION_SIZE   0x00000400
#define SSH_FILEXFER_ATTR_TEXT_HINT         0x00000800
#define SSH_FILEXFER_ATTR_MIME_TYPE         0x00001000
#define SSH_FILEXFER_ATTR_LINK_COUNT        0x00002000
#define SSH_FILEXFER_ATTR_UNTRANSLATED_NAME 0x00004000
#define SSH_FILEXFER_ATTR_CTIME             0x00008000
#define SSH_FILEXFER_ATTR_EXTENDED          0x80000000

#define SSH_FILEXFER_TYPE_REGULAR          1
#define SSH_FILEXFER_TYPE_DIRECTORY        2
#define SSH_FILEXFER_TYPE_SYMLINK          3
#define SSH_FILEXFER_TYPE_SPECIAL          4
#define SSH_FILEXFER_TYPE_UNKNOWN          5
#define SSH_FILEXFER_TYPE_SOCKET           6
#define SSH_FILEXFER_TYPE_CHAR_DEVICE      7
#define SSH_FILEXFER_TYPE_BLOCK_DEVICE     8
#define SSH_FILEXFER_TYPE_FIFO             9

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

#define SSH_FILEXFER_ATTR_FLAGS_READONLY         0x00000001
#define SSH_FILEXFER_ATTR_FLAGS_SYSTEM           0x00000002
#define SSH_FILEXFER_ATTR_FLAGS_HIDDEN           0x00000004
#define SSH_FILEXFER_ATTR_FLAGS_CASE_INSENSITIVE 0x00000008
#define SSH_FILEXFER_ATTR_FLAGS_ARCHIVE          0x00000010
#define SSH_FILEXFER_ATTR_FLAGS_ENCRYPTED        0x00000020
#define SSH_FILEXFER_ATTR_FLAGS_COMPRESSED       0x00000040
#define SSH_FILEXFER_ATTR_FLAGS_SPARSE           0x00000080
#define SSH_FILEXFER_ATTR_FLAGS_APPEND_ONLY      0x00000100
#define SSH_FILEXFER_ATTR_FLAGS_IMMUTABLE        0x00000200
#define SSH_FILEXFER_ATTR_FLAGS_SYNC             0x00000400
#define SSH_FILEXFER_ATTR_FLAGS_TRANSLATION_ERR  0x00000800

#define SSH_FILEXFER_ATTR_KNOWN_TEXT        0x00
#define SSH_FILEXFER_ATTR_GUESSED_TEXT      0x01
#define SSH_FILEXFER_ATTR_KNOWN_BINARY      0x02
#define SSH_FILEXFER_ATTR_GUESSED_BINARY    0x03

#define SSH_FXF_ACCESS_DISPOSITION       0x00000007
#define SSH_FXF_CREATE_NEW           0x00000000
#define SSH_FXF_CREATE_TRUNCATE      0x00000001
#define SSH_FXF_OPEN_EXISTING        0x00000002
#define SSH_FXF_OPEN_OR_CREATE       0x00000003
#define SSH_FXF_TRUNCATE_EXISTING    0x00000004
#define SSH_FXF_APPEND_DATA              0x00000008
#define SSH_FXF_APPEND_DATA_ATOMIC       0x00000010
#define SSH_FXF_TEXT_MODE                0x00000020
#define SSH_FXF_BLOCK_READ               0x00000040
#define SSH_FXF_BLOCK_WRITE              0x00000080
#define SSH_FXF_BLOCK_DELETE             0x00000100
#define SSH_FXF_BLOCK_ADVISORY           0x00000200
#define SSH_FXF_NOFOLLOW                 0x00000400
#define SSH_FXF_DELETE_ON_CLOSE          0x00000800
#define SSH_FXF_ACCESS_AUDIT_ALARM_INFO  0x00001000
#define SSH_FXF_ACCESS_BACKUP            0x00002000
#define SSH_FXF_BACKUP_STREAM            0x00004000
#define SSH_FXF_OVERRIDE_OWNER           0x00008000

#define SSH_FXF_RENAME_OVERWRITE  0x00000001
#define SSH_FXF_RENAME_ATOMIC     0x00000002
#define SSH_FXF_RENAME_NATIVE     0x00000004

#define SSH_FXP_REALPATH_NO_CHECK    0x00000001
#define SSH_FXP_REALPATH_STAT_IF     0x00000002
#define SSH_FXP_REALPATH_STAT_ALWAYS 0x00000003

#define SSH_FXF_READ            0x00000001
#define SSH_FXF_WRITE           0x00000002
#define SSH_FXF_APPEND          0x00000004
#define SSH_FXF_CREAT           0x00000008
#define SSH_FXF_TRUNC           0x00000010
#define SSH_FXF_EXCL            0x00000020
#define SSH_FXF_TEXT            0x00000040

#define SSH_FX_OK                            0
#define SSH_FX_EOF                           1
#define SSH_FX_NO_SUCH_FILE                  2
#define SSH_FX_PERMISSION_DENIED             3
#define SSH_FX_FAILURE                       4
#define SSH_FX_BAD_MESSAGE                   5
#define SSH_FX_NO_CONNECTION                 6
#define SSH_FX_CONNECTION_LOST               7
#define SSH_FX_OP_UNSUPPORTED                8
#define SSH_FX_INVALID_HANDLE                9  /*      v4+ */
#define SSH_FX_NO_SUCH_PATH                  10 /* 0x0A */
#define SSH_FX_FILE_ALREADY_EXISTS           11 /* 0x0B */
#define SSH_FX_WRITE_PROTECT                 12 /* 0x0C */
#define SSH_FX_NO_MEDIA                      13 /* 0x0D */
#define SSH_FX_NO_SPACE_ON_FILESYSTEM        14 /* 0x0E v5+ */
#define SSH_FX_QUOTA_EXCEEDED                15 /* 0x0F */
#define SSH_FX_UNKNOWN_PRINCIPAL             16 /* 0x10 */
#define SSH_FX_LOCK_CONFLICT                 17 /* 0x11 */
#define SSH_FX_DIR_NOT_EMPTY                 18 /* 0x12 v6+ */
#define SSH_FX_NOT_A_DIRECTORY               19 /* 0x13 */
#define SSH_FX_INVALID_FILENAME              20 /* 0x14 */
#define SSH_FX_LINK_LOOP                     21 /* 0x15 */
#define SSH_FX_CANNOT_DELETE                 22 /* 0x16 */
#define SSH_FX_INVALID_PARAMETER             23 /* 0x17 */
#define SSH_FX_FILE_IS_A_DIRECTORY           24 /* 0x18 */
#define SSH_FX_BYTE_RANGE_LOCK_CONFLICT      25 /* 0x19 */
#define SSH_FX_BYTE_RANGE_LOCK_REFUSED       26 /* 0x1A */
#define SSH_FX_DELETE_PENDING                27 /* 0x1B */
#define SSH_FX_FILE_CORRUPT                  28 /* 0x1C */
#define SSH_FX_OWNER_INVALID                 29 /* 0x1D */
#define SSH_FX_GROUP_INVALID                 30 /* 0x1E */
#define SSH_FX_NO_MATCHING_BYTE_RANGE_LOCK   31 /* 0x1F */

#endif /* SFTP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
#define End:
*/
