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


#ifndef TYPES_H
#define TYPES_H

#include <sys/stat.h>
#include <wchar.h>
#include <iconv.h>

struct sftptime {
  int64_t seconds;
  uint32_t nanoseconds;
};

struct sftpattr {
  uint32_t valid;                       /* flags per v6 */
  uint8_t type;
  uint64_t size;
  uint64_t allocation_size;             /* v6+ */
  uint32_t uid, gid;                    /* v3 */ 
  char *owner, *group;                  /* v4+ */
  uint32_t permissions;
  struct sftptime atime;
  struct sftptime createtime;           /* v4+ */
  struct sftptime mtime;
  struct sftptime ctime;                /* v6+ */
  char *acl;                            /* v5+ */
  uint32_t attrib_bits;                 /* v5+ */
  /* all v6+: */
  uint32_t attrib_bits_valid;
  uint8_t text_hint;
  char *mime_type;
  uint32_t link_count;
  char *untranslated_name;
  /* We stuff these in here too so we can conveniently use sftpattrs for
   * name lists */
  const char *name;                     /* still in local encoding */
  const char *longname;
  const wchar_t *wname;                 /* name converted to wide chars */
  const char *target;                   /* link target or 0 */
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
