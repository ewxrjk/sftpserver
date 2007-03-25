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

#ifndef DEBUG_H
#define DEBUG_H

/* Debug support */

extern int sftp_debugging;
extern const char *sftp_debugpath;

void sftp_debug_hexdump(const void *ptr, size_t n);
void sftp_debug_printf(const char *fmt, ...) attribute((format(printf,1,2)));
#define D(x) do {                               \
  if(sftp_debugging)                            \
    sftp_debug_printf x;                        \
} while(0)

#endif /* DEBUG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
