/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2016 Richard Kettlewell
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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LATENCY 10/*seconds*/

static void destroy(const char *path) {
  pid_t pid;
  int w;
  chmod(path, 02775);
  switch(pid = fork()) {
  case 0:
    execlp("rm", "rm", "-rf", path, (char *)NULL);
    _exit(-1);
  case -1:
    fprintf(stderr, "fork: %s\n", strerror(errno));
    return;
  }
  wait(&w);
  if(w)
    fprintf(stderr, "rm: wait status %#x\n", w);
}

static void maybe_clean_entry(const char *tmproot, const char *name) {
  struct stat sb;
  size_t sz = strlen(tmproot) + strlen(name) + 2;
  char *b = alloca(sz);
  snprintf(b, sz, "%s/%s", tmproot, name);
  if(stat(b, &sb) < 0)
    return;
  if(!S_ISDIR(sb.st_mode) < 0)
    return;
  if(sb.st_ctime > time(NULL) - LATENCY)
    return;
  destroy(b);
}

static void clean_directory(const char *tmproot) {
  struct dirent *de;
  DIR *dp = opendir(tmproot);
  if(!dp) {
    fprintf(stderr, "opendir %s: %s\n", tmproot, strerror(errno));
    exit(1);
  }
  while((de = readdir(dp))) {
    if(de->d_name[0] == '.')
      continue;
    maybe_clean_entry(tmproot, de->d_name);
  }
  closedir(dp);
}

int main(int argc, char **argv) {
  const char *tmproot;

  if(argc != 2) {
    fprintf(stderr, "usage: %s PATH\n", argv[0]);
    return 1;
  }
  tmproot = argv[1];
  for(;;) {
    clean_directory(tmproot);
    sleep(1);
  }
}
