/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2010, 2011 Richard Kettlewell
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
#include "utils.h"
#include "debug.h"
#include "alloc.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <limits.h>
#include <assert.h>

int sftp_log_syslog;

int sftp_xread(int fd, void *buffer, size_t size) {
  size_t sofar = 0;
  ssize_t n;
  char *ptr = buffer;

  while(sofar < size) {
    n = read(fd, ptr + sofar, size - sofar);
    if(n > 0)
      sofar += n;
    else if(n == 0)
      return -1; /* eof */
    else
      sftp_fatal("read error: %s", strerror(errno));
  }
  return 0; /* ok */
}

void *sftp_xmalloc(size_t n) {
  void *ptr;

  if(!n)
    n = 1;
  if(!(ptr = malloc(n)))
    sftp_fatal("sftp_xmalloc: out of memory (%zu)", n);
  return ptr;
}

void *sftp_xcalloc(size_t n, size_t size) {
  void *ptr;

  if(!n)
    n = 1;
  if(!size)
    size = 1;
  if(!(ptr = calloc(n, size)))
    sftp_fatal("sftp_xcalloc: out of memory (%zu, %zu)", n, size);
  return ptr;
}

void *sftp_xrecalloc(void *ptr, size_t n, size_t size) {
  if(n > SIZE_MAX / size)
    sftp_fatal("sftp_xrecalloc: out of memory (%zu, %zu)", n, size);
  n *= size;
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      sftp_fatal("sftp_xrecalloc: out of memory (%zu)", n);
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

void *sftp_xrealloc(void *ptr, size_t n) {
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      sftp_fatal("sftp_xrealloc: out of memory (%zu)", n);
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

char *sftp_xstrdup(const char *s) {
  return strcpy(sftp_xmalloc(strlen(s) + 1), s);
}

static void (*exitfn)(int) attribute((noreturn)) = exit;

void sftp_fatal(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  if(sftp_log_syslog)
    vsyslog(LOG_ERR, msg, ap);
  else {
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, msg, ap);
    fputc('\n', stderr);
  }
  va_end(ap);
  exitfn(-1);
}

void sftp_forked(void) { exitfn = _exit; }

pid_t sftp_xfork(void) {
  pid_t pid;

  if((pid = fork()) < 0)
    sftp_fatal("fork: %s", strerror(errno));
  if(!pid)
    sftp_forked();
  return pid;
}

char *sftp_str_appendn(struct allocator *a, char *s, size_t *ns, const char *t,
                       size_t lt) {
  const size_t ls = s ? strlen(s) : 0, need = lt + ls + 1;

  if(need > *ns) {
    size_t newsize = *ns ? *ns : 16;

    while(need > newsize && newsize)
      newsize *= 2;
    if(!newsize)
      sftp_fatal("sftp_str_appendn: out of memory (%zu, %zu)", ls, need);
    s = sftp_alloc_more(a, s, *ns, newsize);
    *ns = newsize;
  } else {
    // need should always be at least 1 so need<=*ns => *ns > 0 => s != 0.
    assert(s);
  }

  sftp_memcpy(s + ls, t, lt);
  s[ls + lt] = 0;
  return s;
}

char *sftp_str_append(struct allocator *a, char *s, size_t *ns, const char *t) {
  return sftp_str_appendn(a, s, ns, t, strlen(t));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
