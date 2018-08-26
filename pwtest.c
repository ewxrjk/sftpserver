/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2014 Richard Kettlewell
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
#include "putword.h"
#include <string.h>
#include <assert.h>

int main() {
  unsigned char buffer[16];
  int n, m;

  for(n = 0; n <= 14; ++n) {
    memset(buffer, 0xAF, 16);
    put16(buffer + n, 0x0102);
    assert(get16(buffer + n) == 0x0102);
    for(m = 0; m < 16; ++m) {
      if(m >= n && m < n + 2)
        assert(buffer[m] == 1 + m - n);
      else
        assert(buffer[m] == 0xAF);
    }
  }

  for(n = 0; n <= 12; ++n) {
    memset(buffer, 0xAF, 16);
    put32(buffer + n, 0x01020304);
    // assert(get32(buffer + n) == 0x01020304);
    for(m = 0; m < 16; ++m) {
      if(m >= n && m < n + 4)
        assert(buffer[m] == 1 + m - n);
      else
        assert(buffer[m] == 0xAF);
    }
  }

  for(n = 0; n <= 8; ++n) {
    memset(buffer, 0xAF, 16);
    put64(buffer + n, 0x0102030405060708ULL);
    // assert(get64(buffer + n) == 0x0102030405060708ULL);
    for(m = 0; m < 16; ++m) {
      if(m >= n && m < n + 8)
        assert(buffer[m] == 1 + m - n);
      else
        assert(buffer[m] == 0xAF);
    }
  }

  return 0;
}
