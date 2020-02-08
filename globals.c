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
#include "types.h"
#include "globals.h"
#include "thread.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#if NTHREADS > 1
struct queue *workqueue = 0;
#endif

static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static enum sftp_state state;

void sftp_state_set(enum sftp_state s) {
  ferrcheck(pthread_mutex_lock(&state_lock));
  state = s;
  ferrcheck(pthread_mutex_unlock(&state_lock));
}

enum sftp_state sftp_state_get(void) {
  enum sftp_state r;
  ferrcheck(pthread_mutex_lock(&state_lock));
  r = state;
  ferrcheck(pthread_mutex_unlock(&state_lock));
  return r;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
