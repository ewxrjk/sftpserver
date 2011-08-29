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

/** @file types.h @brief Data types */

#ifndef TYPES_H
#define TYPES_H

#include <sys/stat.h>
#include <wchar.h>
#include <iconv.h>

/** @brief An SFTP timestamp */
struct sftptime {
  /** @brief Seconds since Jan 1 1970 UTC
   *
   * Presumably excludes leap-seconds per usual Unix convention.
   */
  int64_t seconds;

  /** @brief Nanoseconds
   *
   * May be forced to 0 if the implementation does not support high-resolution
   * file timestamps. */
  uint32_t nanoseconds;
};

/** @brief SFTP file attributes
 *
 * In this structure V6-compatible definitions are used; particular protocol
 * implementations must convert when (de-)serializing.  Many of the fields are
 * only valid if an appropriate bit is set in the @c valid field.
 */
struct sftpattr {
  /** @brief Validity mask
   *
   * Present in all versions, but the V6 bit values are always used here.  See
   * @ref valid_attribute_flags for a list of bits. */
  uint32_t valid;

  /** @brief File type
   *
   * Always valid.  See @ref file_type for a list of bits. */
  uint8_t type;

  /** @brief File size
   *
   * Only if @ref SSH_FILEXFER_ATTR_SIZE is set. */
  uint64_t size;

  /** @brief Allocation size (i.e. total bytes consumed)
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_ALLOCATION_SIZE is set. */
  uint64_t allocation_size;

  /** @brief File owner UID
   *
   * Only v3. */
  uint32_t uid;

  /** @brief File group GID
   *
   * Only v3. */
  uint32_t gid;

  /** @brief File owner name
   *
   * Only v4+, only if @ref SSH_FILEXFER_ATTR_OWNERGROUP is set.
   */
  char *owner;

  /** @brief File group name
   *
   * Only v4+, only if @ref SSH_FILEXFER_ATTR_OWNERGROUP is set.
   */
  char *group;

  /** @brief File permissions
   *
   * Only if @ref SSH_FILEXFER_ATTR_PERMISSIONS is set.  See @ref permissions
   * for a list of bits.
   */
  uint32_t permissions;

  /** @brief File access time
   *
   * Only if @ref SSH_FILEXFER_ATTR_ACCESSTIME is set.
   */
  struct sftptime atime;

  /** @brief File create time
   *
   * Only v4+, only if @ref SSH_FILEXFER_ATTR_CREATETIME is set.
   */
  struct sftptime createtime;

  /** @brief File modification time
   *
   * Only if @ref SSH_FILEXFER_ATTR_MODIFYTIME is set.
   */
  struct sftptime mtime;

  /** @brief File inode change time
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_CTIME is set.
   */
  struct sftptime ctime;

  /** @brief File ACL
   *
   * ACLs are not really supported.  We parse them but ignore them.
   *
   * Only v5+, only if @ref SSH_FILEXFER_ATTR_ACL is set.
   */
  char *acl;

  /** @brief Attribute bits
   *
   * Only v5+, only if @ref SSH_FILEXFER_ATTR_BITS.
   * See @ref attrib_bits for bits.
   */
  uint32_t attrib_bits;

  /* all v6+: */

  /** @brief Validity mask for attribute bits
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_BITS.
   * See @ref attrib_bits for bits.
   */
  uint32_t attrib_bits_valid;

  /** @brief Server's knowledge about file contents
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_TEXT_HINT.
   * See @ref text_hint for bits.
   */
  uint8_t text_hint;

  /** @brief File MIME type
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_MIME_TYPE.
   */
  char *mime_type;

  /** @brief File link count
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_LINK_COUNT.
   */
  uint32_t link_count;

  /** @brief Untranslated form of name
   *
   * Only v6+, only if @ref SSH_FILEXFER_ATTR_UNTRANSLATED_NAME.
   */
  char *untranslated_name;

  /* We stuff these in here too so we can conveniently use sftpattrs for
   * name lists */

  /** @brief Local encoding of filename
   *
   * Not part of the SFTP attributes.  Just used for convenience by a number of
   * functions.
   *
   * @todo Document exactly how sftpattrs::name is used.
   */
  const char *name;

  /** @brief V3 @c longname file description
   *
   * Not part of the SFTP attributes.  Instead used by the client to capture
   * the @c longname field sent in v3 @ref SSH_FXP_NAME responses. */
  const char *longname;

  /** @brief Local wide encoding of filename
   *
   * Equivalent to @c name but converted to wide characters. */
  const wchar_t *wname;

  /** @brief Symbolic link target
   *
   * Not part of the SFTP attributes.  Instead used by the client and
   * sftp_format_attr() to capture the target of a symbolic link. */
  const char *target;
};
/* SFTP-style file attributes */

struct worker {
  size_t bufsize, bufused;
  uint8_t *buffer;
  iconv_t local_to_utf8, utf8_to_local;
};
/* Thread-specific data */

struct sftpjob {
  size_t len;
  unsigned char *data;                  /* whole job */
  size_t left;
  const unsigned char *ptr;             /* unparsed portion of job */
  struct allocator *a;                  /* allocator */
  uint32_t id;                          /* ID or 0 */
  struct worker *worker;                /* worker processing this job */
};
/* An SFTP job. */

struct sftpcmd {
  uint8_t type;                         /* message type */
  uint32_t (*handler)(struct sftpjob *job);
};

struct sftpextension {
  const char *name;
  uint32_t (*handler)(struct sftpjob *job);
};

#define HANDLER_RESPONDED ((uint32_t)-1) /* already responded */
#define HANDLER_ERRNO ((uint32_t)-2)     /* send an errno status */

struct sftpprotocol {
  int ncommands;
  const struct sftpcmd *commands;       /* sorted by type */
  int version;                          /* protocol version number */
  uint32_t attrmask;                    /* known attr valid mask */
  uint32_t maxstatus;                   /* max known status */
  void (*sendnames)(struct sftpjob *job, 
                    int nnames, const struct sftpattr *names);
  void (*sendattrs)(struct sftpjob *job, const struct sftpattr *filestat);
  uint32_t (*parseattrs)(struct sftpjob *job, struct sftpattr *filestat);
  int (*encode)(struct sftpjob *job,
                char **path);           /* Convert from local to wire */
  uint32_t (*decode)(struct sftpjob *job,
                     char **path);      /* Convert from wire to local */
  int nextensions;
  const struct sftpextension *extensions;
};
/* An SFTP protocol version */

#endif /* TYPES_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
