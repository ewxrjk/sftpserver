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

#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <stdio.h>

/* Error-checking for pthreads functions */
#define ferrcheck(E) do {                                       \
  const int frc = (E);                                          \
  if(frc) {                                                     \
    fatal("%s:%d: %s: %s\n", __FILE__, __LINE__,                \
          #E, strerror(frc));                                   \
    exit(1);                                                    \
  }                                                             \
} while(0)

#endif /* THREAD_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
