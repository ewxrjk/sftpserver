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

int log_syslog;

int do_read(int fd, void *buffer, size_t size) {
  size_t sofar = 0;
  ssize_t n;
  char *ptr = buffer;

  while(sofar < size) {
    n = read(fd, ptr + sofar, size - sofar);
    if(n > 0)
      sofar += n;
    else if(n == 0)
      return -1;                        /* eof */
    else
      fatal("read error: %s", strerror(errno));
  }
  return 0;                             /* ok */
}

void *xmalloc(size_t n) {
  void *ptr;

  if(n) {
    if(!(ptr = malloc(n)))
      fatal("out of memory");
    return ptr;
  } else
    return 0;
}

void *xcalloc(size_t n, size_t size) {
  void *ptr;

  if(n && size) {
    if(!(ptr = calloc(n, size)))
      fatal("out of memory");
    return ptr;
  } else
    return 0;
}

void *xrecalloc(void *ptr, size_t n, size_t size) {
  if(n > SIZE_MAX / size)
      fatal("out of memory");
  n *= size;
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      fatal("out of memory");
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

void *xrealloc(void *ptr, size_t n) {
  if(n) {
    if(!(ptr = realloc(ptr, n)))
      fatal("out of memory");
    return ptr;
  } else {
    free(ptr);
    return 0;
  }
}

char *xstrdup(const char *s) {
  return strcpy(xmalloc(strlen(s) + 1), s);
}

static void (*exitfn)(int) attribute((noreturn)) = exit;

void fatal(const char *msg, ...) {
  va_list ap;

  va_start(ap, msg);
  if(log_syslog)
    vsyslog(LOG_ERR, msg, ap);
  else {
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, msg, ap);
    fputc('\n', stderr);
  }
  va_end(ap);
  exitfn(-1);
}

void forked(void) {
  exitfn = _exit;
}

pid_t xfork(void) {
  pid_t pid;

  if((pid = fork()) < 0)
    fatal("fork: %s", strerror(errno));
  if(!pid)
    forked();
  return pid;
}

char *appendn(struct allocator *a, char *s, size_t *ns,
              const char *t, size_t lt) {
  const size_t ls = s ? strlen(s) : 0, need = lt + ls + 1;
  
  if(need > *ns) {
    size_t newsize = *ns ? *ns : 16;

    while(need > newsize && newsize)
      newsize *= 2;
    if(!newsize)
      fatal("out of memory");
    s = sftp_alloc_more(a, s, *ns, newsize);
    *ns = newsize;
  } else {
    // need should always be at least 1 so need<=*ns => *ns > 0 => s != 0.
    assert(s);
  }

  memcpy(s + ls, t, lt);
  s[ls + lt] = 0;
  return s;
}

char *append(struct allocator *a, char *s, size_t *ns, 
             const char *t) {
  return appendn(a, s, ns, t, strlen(t));
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
