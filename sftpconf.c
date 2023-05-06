/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) Richard Kettlewell
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
#include "sftpconf.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

int sftpconf_nthreads = NTHREADS;
int sftpconf_reorder = 1;

static size_t sftpconf_split(char *line, char **words, size_t maxwords) {
  size_t nwords = 0;
  char *buffer = sftp_xmalloc(strlen(line) + 1), *word;

  // Keep going until we hit the end of the line, or start of a comment
  while(*line && *line != '#') {
    if(isspace((unsigned char)*line)) {
      ++line;
      continue;
    }
    if(*line == '"' || *line == '\'') {
      // Quoted word
      int quote = *line++; // Step over quote
      word = buffer;
      while(*line && *line != quote) {
        if(*line == '\\') // Backslash escapes anything
          line++;
        *buffer++ = *line++;
      }
      if(*line) // Step over final quote if present
        line++;
      *buffer++ = 0;
    } else {
      // Unquoted word
      word = buffer;
      while(*line && !isspace((unsigned char)*line))
        *buffer++ = *line++;
      *buffer++ = 0;
    }
    if(nwords >= maxwords)
      return -1;
    words[nwords++] = word;
  }
  return nwords;
}

void sftpconf_read(const char *path) {
  FILE *fp = NULL;
  char line[1024];
  int lineno = 0;
  char *words[8];

  if(!(fp = fopen(path, "r"))) {
    if(errno == ENOENT)
      return;
    sftp_fatal("%s: %s", path, strerror(errno));
  }
  while(fgets(line, sizeof line, fp)) {
    lineno++;
    if(!strchr(line, '\n'))
      sftp_fatal("%s:%d: line too long", path, lineno);
    size_t nwords = sftpconf_split(line, words, sizeof words / sizeof *words);
    if(nwords == (size_t)-1)
      sftp_fatal("%s:%d: too many parts to line", path, lineno);
    if(nwords == 0)
      continue;
    if(!strcmp(words[0], "threads")) {
      if(nwords != 2)
        sftp_fatal("%s:%d: invalid threads directive", path, lineno);
      sftpconf_nthreads = atoi(words[1]);
    } else if(!strcmp(words[0], "reorder")) {
      if(nwords != 2)
        sftp_fatal("%s:%d: invalid reorder directive", path, lineno);
      if(!strcmp(words[1], "true"))
        sftpconf_reorder = 1;
      else if(!strcmp(words[1], "false"))
        sftpconf_reorder = 0;
      else
        sftp_fatal("%s:%d: invalid reorder directive", path, lineno);
    } else {
      sftp_fatal("%s:%d: unrecognized directive: %s", path, lineno, words[0]);
    }
  }
  fclose(fp);
}
