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

#include "sftpcommon.h"
#include "utils.h"
#include "charset.h"
#include "alloc.h"
#include "debug.h"
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

wchar_t *sftp_mbs2wcs(const char *s) {
  wchar_t *ws;
  size_t len;
  mbstate_t ps;
  
  memset(&ps, 0, sizeof ps);
  len = mbsrtowcs(0, &s, 0, &ps);
  if(len == (size_t)-1)
    return 0;
  ws = xcalloc(len + 1, sizeof *ws);
  memset(&ps, 0, sizeof ps);
  mbsrtowcs(ws, &s, len, &ps);
  return ws;
}

int sftp_iconv(struct allocator *a, iconv_t cd, char **sp) {
  const char *const input = *sp;
  const size_t inputsize = strlen(input);
  size_t outputsize = 2 * strlen(input) + 1;
  size_t rc;
  const char *inbuf;
  char *outbuf, *output;
  size_t inbytesleft, outbytesleft;
  
  assert(cd != 0);
  do {
    output = sftp_alloc(a, outputsize);
    iconv(cd, 0, 0, 0, 0);
    inbuf = input;
    inbytesleft = inputsize;
    outbuf = output;
    outbytesleft = outputsize;
    rc = iconv(cd, (void *)&inbuf, &inbytesleft, &outbuf, &outbytesleft);
    outputsize *= 2;
  } while(rc == (size_t)-1 && errno == E2BIG);
  if(rc == (size_t)-1)
    return -1;
  *sp = output;
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
