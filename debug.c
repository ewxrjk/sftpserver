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

#include "debug.h"
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>

static FILE *debugfp;
const char *sftp_debugpath;
int sftp_debugging;

static void opendebug(void) {
  assert(sftp_debugging);
  if(!debugfp) {
    if(sftp_debugpath) {
      int fd;

      if((fd = open(sftp_debugpath, O_WRONLY|O_CREAT|O_TRUNC, 0600)) >= 0)
        debugfp = fdopen(fd, "w");
      else
        fprintf(stderr, "%s: %s\n", sftp_debugpath, strerror(errno));
    }
    if(!debugfp)
      debugfp = stderr;
  }
}

void sftp_debug_hexdump(const void *ptr, size_t n) {
  const unsigned char *p = ptr;
  size_t i, j;
  char buffer[80], *output;

  opendebug();
  for(i = 0; i < n; i += 16) {
    output = buffer;
    output += sprintf(output, "%4lx ", (unsigned long)i);
    for(j = 0; j < 16; ++j)
      if(i + j < n)
	output += sprintf(output, " %02x", p[i + j]);
      else {
        strcpy(output, "   ");
        output += 3;
      }
    strcpy(output, "  ");
    output += 2;
    for(j = 0; j < 16; ++j)
      if(i + j < n)
        *output++ = isprint(p[i + j]) ? p[i+j] : '.';
    *output++ = '\n';
    *output = 0;
    fputs(buffer, debugfp);
  }
  fflush(debugfp);
}

void sftp_debug_printf(const char *fmt, ...) {
  va_list ap;
  const int save_errno = errno;

  opendebug();
  va_start(ap, fmt);
  vfprintf(debugfp, fmt, ap);
  va_end(ap);
  fputc('\n', debugfp);
  fflush(debugfp);
  errno = save_errno;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
