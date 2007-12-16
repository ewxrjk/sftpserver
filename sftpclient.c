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

#include "sftpclient.h"
#include "utils.h"
#include "xfns.h"
#include "send.h"
#include "globals.h"
#include "types.h"
#include "alloc.h"
#include "parse.h"
#include "sftp.h"
#include "debug.h"
#include "thread.h"
#include "stat.h"
#include "charset.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <libgen.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <locale.h>
#include <sys/time.h>
#include <langinfo.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <limits.h>
#include <termios.h>
#if HAVE_READLINE
# include <readline/readline.h>
# include <readline/history.h>
#endif

struct command {
  const char *name;
  int minargs, maxargs;
  int (*handler)(int ac, char **av);
  const char *args;
  const char *help;
};

struct handle {
  size_t len;
  char *data;
};

static int sftpin;
static struct allocator allocator;
static struct sftpjob fakejob;
static struct worker fakeworker;
static char *cwd;
static const char *inputpath;
static int inputline;
static int progress_indicators = 1;
static int terminal_width;
static int textmode;
static const char *newline = "\r\n";
static const char *vendorname, *servername, *serverversion, *serverversions;
static uint64_t serverbuild;
static int stoponerror;
static int echo;
static uint32_t attrmask;

const struct sftpprotocol *protocol = &sftp_v3;
const char sendtype[] = "request";

/* Command line */
static size_t buffersize = 32768;
static int nrequests = 16;
static const char *subsystem;
static const char *program;
static const char *batchfile;
static int sshversion;
static int compress;
static const char *sshconf;
static const char *sshoptions[1024];
static int nsshoptions;
static int sshverbose;
static int sftpversion = 6;
static int quirk_reverse_symlink;
static int forceversion;

static char *sftp_realpath(const char *path);

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "buffer", required_argument, 0, 'B' },
  { "batch", required_argument, 0, 'b' },
  { "program", required_argument, 0, 'P' },
  { "requests", required_argument, 0, 'R' },
  { "subsystem", required_argument, 0, 's' },
  { "sftp-version", required_argument, 0, 'S' },
  { "quirk-reverse-symlink", no_argument, 0, 256 },
  { "stop-on-error", no_argument, 0, 257 },
  { "no-stop-on-error", no_argument, 0, 258 },
  { "progress", no_argument, 0, 259 },
  { "no-progress", no_argument, 0, 260 },
  { "echo", no_argument, 0, 261 },
  { "fix-sigpipe", no_argument, 0, 262 },
  { "force-version", required_argument, 0, 263 },
  { "debug", no_argument, 0, 'd' },
  { "debug-path", required_argument, 0, 'D' },
  { "host", required_argument, 0, 'H' },
  { "port", required_argument, 0, 'p' },
  { "ipv4", no_argument, 0, '4' },
  { "ipv6", no_argument, 0, '6' },
  { "1", no_argument, 0, '1' },
  { "2", no_argument, 0, '2' },
  { "C", no_argument, 0, 'C' },
  { "F", required_argument, 0, 'F' },
  { "o", required_argument, 0, 'o' },
  { "v", no_argument, 0, 'v' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
          "  sftpclient [OPTIONS] [USER@]HOST\n"
          "\n"
          "Quick and dirty SFTP client\n"
          "\n");
  xprintf("Options:\n"
          "  --help, -h               Display usage message\n"
          "  --version, -V            Display version number\n"
          "  -B, --buffer BYTES       Select buffer size (default 8192)\n"
          "  -b, --batch PATH         Read batch file\n"
          "  -P, --program PATH       Execute program as SFTP server\n");
  xprintf("  -R, --requests COUNT     Maximum outstanding requests (default 8)\n"
          "  -s, --subsystem NAME     Remote subsystem name\n"
          "  -S, --sftp-version VER   Protocol version to request (default 3)\n"
          "  --quirk-openssh          Server gets SSH_FXP_SYMLINK backwards\n");
  xprintf("Options passed to SSH:\n"
          "  -1, -2                   Select protocol version\n"
          "  -C                       Enable compression\n"
          "  -F PATH                  Use alternative  config file\n"
          "  -o OPTION                Pass option to client\n"
          "  -v                       Raise logging level\n");
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("sftp client version %s\n", VERSION);
  exit(0);
}

/* Utilities */

static int attribute((format(printf,1,2))) error(const char *fmt, ...) {
  va_list ap;

  if(inputpath)
    fprintf(stderr, "%s:%d ", inputpath, inputline);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n',  stderr);
  fflush(stderr);
  return -1;
}

static int status(void) {
  uint32_t status;
  char *msg;
  
  /* Cope with half-parsed responses */
  fakejob.ptr = fakejob.data + 5;
  fakejob.left = fakejob.len - 5;
  cpcheck(sftp_parse_uint32(&fakejob, &status));
  cpcheck(sftp_parse_string(&fakejob, &msg, 0));
  if(status) {
    error("%s (%s)", msg, status_to_string(status));
    return -1;
  } else
    return 0;
}

/* Get a response.  Picks out the type and ID. */
static uint8_t getresponse(int expected, uint32_t expected_id,
                           const char *what) {
  uint32_t len;
  uint8_t type;

  if(do_read(sftpin, &len, sizeof len))
    fatal("unexpected EOF from server while reading %s response length", what);
  free(fakejob.data);                   /* free last job */
  fakejob.len = ntohl(len);
  fakejob.data = xmalloc(fakejob.len);
  if(do_read(sftpin, fakejob.data, fakejob.len))
    fatal("unexpected EOF from server while reading %s response data", what);
  if(sftp_debugging) {
    D(("%s response:", what));
    sftp_debug_hexdump(fakejob.data, fakejob.len);
  }
  fakejob.left = fakejob.len;
  fakejob.ptr = fakejob.data;
  cpcheck(sftp_parse_uint8(&fakejob, &type));
  if(type != SSH_FXP_VERSION) {
    cpcheck(sftp_parse_uint32(&fakejob, &fakejob.id));
    if(expected_id && fakejob.id != expected_id)
      fatal("wrong ID in response to %s (want %"PRIu32
            " got %"PRIu32" type was %d)",
            what, expected_id, fakejob.id, type);
  }
  if(expected > 0 && type != expected) {
    if(type == SSH_FXP_STATUS)
      status();
    else
      fatal("expected %s response %d got %d", what, expected, type);
  }
  return type;
}

/* Split a command line */
static int split(char *line, char **av) {
  char *arg;
  int ac = 0;

  while(*line) {
    if(isspace((unsigned char)*line)) {
      ++line;
      continue;
    }
    if(*line == '"') {
      arg = av[ac++] = line++;
      while(*line && *line != '"') {
	if(*line == '\\' && line[1])
	  ++line;
	*arg++ = *line++;
      }
      if(!*line) {
	error("unterminated string");
	return -1;
      }
      *arg++ = 0;
      line++;
    } else {
      av[ac++] = line;
      while(*line && !isspace((unsigned char)*line))
	++line;
      if(*line)
	*line++ = 0;
    }
  }
  av[ac] = 0;
  return ac;
}

static uint32_t newid(void) {
  static uint32_t latestid;

  do {
    ++latestid;
  } while(!latestid);
  return latestid;
}

/* Find path to current directory */
static char *remote_cwd(void) {
  if(!cwd) {
    if(!(cwd = sftp_realpath(".")))
      exit(1);
    cwd = xstrdup(cwd);
  }
  return cwd;
}

static const char *makeabspath(const char *name) {
  char *resolved;

  assert(cwd != 0);
  if(name[0] == '/')
    return name;
  resolved = sftp_alloc(fakejob.a, strlen(cwd) + strlen(name) + 2);
  sprintf(resolved, "%s/%s", cwd, name);
  return resolved;
}

static void progress(const char *path, uint64_t sofar, uint64_t total) {
  if(progress_indicators) {
    if(!total)
      xprintf("\r%*s\r", terminal_width, "");
    else if(total == (uint64_t)-1)
      xprintf("\r%.60s: %12"PRIu64"b", path, sofar);
    else
      xprintf("\r%.60s: %12"PRIu64"b %3d%%",
              path, sofar, (int)(100 * sofar / total));
    if(fflush(stdout) < 0) 
      fatal("error writing to stdout: %s", strerror(errno));
  }
}

/* SFTP operation stubs */

static int sftp_init(void) {
  uint32_t version, u32;
  uint16_t u16;
  
  /* Send SSH_FXP_INIT */
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_INIT);
  sftp_send_uint32(&fakeworker, sftpversion);
  sftp_send_end(&fakeworker);
  
  /* Parse the version reponse */
  if(getresponse(SSH_FXP_VERSION, 0, "SSH_FXP_INIT") != SSH_FXP_VERSION)
    return -1;
  cpcheck(sftp_parse_uint32(&fakejob, &version));
  switch(version) {
  case 3:
    protocol = &sftp_v3;
    break;
  case 4:
    protocol = &sftp_v4;
    break;
  case 5:
    protocol = &sftp_v5;
    break;
  case 6:
    protocol = &sftp_v6;
    break;
  default:
    return error("server wanted protocol version %"PRIu32, version);
  }
  attrmask = protocol->attrmask;
  /* Extension data */
  while(fakejob.left) {
    char *xname, *xdata;
    size_t xdatalen;

    cpcheck(sftp_parse_string(&fakejob, &xname, 0));
    cpcheck(sftp_parse_string(&fakejob, &xdata, &xdatalen));
    D(("server sent extension '%s'", xname));
    /* TODO table-driven extension parsing */
    if(!strcmp(xname, "newline")) {
      newline = xstrdup(xdata);
      if(!*newline)
        return error("cannot cope with empty newline sequence");
      /* TODO check newline sequence doesn't contain repeats */
    } else if(!strcmp(xname, "vendor-id")) {
      struct sftpjob xjob;
      
      xjob.a = &allocator;
      xjob.ptr = (void *)xdata;
      xjob.left = xdatalen;
      cpcheck(sftp_parse_string(&xjob, (char **)&vendorname, 0));
      cpcheck(sftp_parse_string(&xjob, (char **)&servername, 0));
      cpcheck(sftp_parse_string(&xjob, (char **)&serverversion, 0));
      cpcheck(sftp_parse_uint64(&xjob, &serverbuild));
      vendorname = xstrdup(vendorname);
      servername = xstrdup(servername);
      serverversion = xstrdup(serverversion);
    } else if(!strcmp(xname, "versions"))
      serverversions = xstrdup(xdata);
    else if(!strcmp(xname, "symlink-order@rjk.greenend.org.uk")) {
      /* See commentary in v3.c */
      if(!strcmp(xdata,  "targetpath-linkpath"))
        quirk_reverse_symlink = 1;
      else if(!strcmp(xdata, "linkpath-targetpath"))
        quirk_reverse_symlink = 0;
      else
        error("unknown %s value '%s'", xname, xdata);
    } else if(!strcmp(xname, "supported")) {
      struct sftpjob xjob;
      
      xjob.a = &allocator;
      xjob.ptr = (void *)xdata;
      xjob.left = xdatalen;
      cpcheck(sftp_parse_uint32(&xjob, &attrmask));
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-attribute-bits */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-open-flags */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-access-mask */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* max-read-size */
      while(xjob.left)
        cpcheck(sftp_parse_string(&xjob, 0, 0)); /* extension-names */
    } else if(!strcmp(xname, "supported2")) {
      struct sftpjob xjob;
      
      xjob.a = &allocator;
      xjob.ptr = (void *)xdata;
      xjob.left = xdatalen;
      cpcheck(sftp_parse_uint32(&xjob, &attrmask)); /* supported-attribute-mask */
      assert(!(attrmask & SSH_FILEXFER_ATTR_CTIME));
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-attribute-bits */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-open-flags */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* supported-access-mask */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* max-read-size */
      cpcheck(sftp_parse_uint16(&xjob, &u16)); /* supported-open-block-vector */
      cpcheck(sftp_parse_uint16(&xjob, &u16)); /* supported-block-vector */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* attrib-extension-count */
      while(u32-- > 0)
        cpcheck(sftp_parse_string(&xjob, 0, 0)); /* attrib-extension-names */
      cpcheck(sftp_parse_uint32(&xjob, &u32)); /* extension-count */
      while(u32-- > 0)
        cpcheck(sftp_parse_string(&xjob, 0, 0)); /* extension-names */
    }
  }
  /* Make sure outbound translation will actually work */
  if(buffersize < strlen(newline))
    buffersize = strlen(newline);
  return 0;
}

static char *sftp_realpath(const char *path) {
  char *resolved;
  uint32_t u32, id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_REALPATH);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, path);
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_NAME, id, "SSH_FXP_REALPATH") != SSH_FXP_NAME)
    return 0;
  cpcheck(sftp_parse_uint32(&fakejob, &u32));
  if(u32 != 1)
    fatal("wrong count in SSH_FXP_REALPATH reply");
  cpcheck(sftp_parse_path(&fakejob, &resolved));
  return resolved;
}

static char *sftp_realpath_v6(const char *path,
                              int control_byte,
                              char **compose,
                              struct sftpattr *attrs) {
  char *resolved;
  uint32_t u32, id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_REALPATH);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, path);
  if(control_byte >= 0) {
    sftp_send_uint8(&fakeworker, control_byte);
    if(compose)
      while(*compose)
        sftp_send_path(&fakejob, &fakeworker, *compose++);
  }
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_NAME, id, "SSH_FXP_REALPATH") != SSH_FXP_NAME)
    return 0;
  cpcheck(sftp_parse_uint32(&fakejob, &u32));
  if(u32 != 1)
    fatal("wrong count in SSH_FXP_REALPATH reply");
  cpcheck(sftp_parse_path(&fakejob, &resolved));
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  return resolved;
}

static int sftp_stat(const char *path, struct sftpattr *attrs,
                     uint8_t type) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, type);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  if(protocol->version > 3)
    sftp_send_uint32(&fakeworker, 0xFFFFFFFF);
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_ATTRS, id, "SSH_FXP_*STAT") != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  attrs->name = path;
  attrs->wname = sftp_mbs2wcs(attrs->name);
  return 0;
}

static int sftp_fstat(const struct handle *hp, struct sftpattr *attrs) {
  uint32_t id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_FSTAT);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_bytes(&fakeworker, hp->data, hp->len);
  if(protocol->version > 3)
    sftp_send_uint32(&fakeworker, 0xFFFFFFFF);
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_ATTRS, id, "SSH_FXP_FSTAT") != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  return 0;
}

static int sftp_opendir(const char *path, struct handle *hp) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_OPENDIR);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_HANDLE, id, "SSH_FXP_OPENDIR") != SSH_FXP_HANDLE)
    return -1;
  cpcheck(sftp_parse_string(&fakejob, &hp->data, &hp->len));
  return 0;
}

static int sftp_readdir(const struct handle *hp,
                        struct sftpattr **attrsp,
                        size_t *nattrsp) {
  uint32_t id, n;
  struct sftpattr *attrs;
  char *name, *longname = 0;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READDIR);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_bytes(&fakeworker, hp->data, hp->len);
  sftp_send_end(&fakeworker);
  switch(getresponse(-1, id, "SSH_FXP_READDIR")) {
  case SSH_FXP_NAME:
    cpcheck(sftp_parse_uint32(&fakejob, &n));
    if(n > SIZE_MAX / sizeof(struct sftpattr))
      fatal("too many attributes in SSH_FXP_READDIR response");
    attrs = sftp_alloc(fakejob.a, n * sizeof(struct sftpattr));
    *nattrsp = n;
    *attrsp = attrs;
    while(n-- > 0) {
      cpcheck(sftp_parse_path(&fakejob, &name));
      if(protocol->version <= 3)
        cpcheck(sftp_parse_path(&fakejob, &longname));
      cpcheck(protocol->parseattrs(&fakejob, attrs));
      attrs->name = name;
      attrs->longname = longname;
      attrs->wname = sftp_mbs2wcs(attrs->name);
      ++attrs;
    }
    return 0;
  case SSH_FXP_STATUS:
    cpcheck(sftp_parse_uint32(&fakejob, &n));
    if(n == SSH_FX_EOF) {
      *nattrsp = 0;
      *attrsp = 0;
      return 0;
    }
    status();
    return -1;
  default:
    fatal("bogus response to SSH_FXP_READDIR");
  }
}

static int sftp_close(const struct handle *hp) {
  uint32_t id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_CLOSE);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_bytes(&fakeworker, hp->data, hp->len);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_CLOSE");
  return status();
}

static int sftp_setstat(const char *path,
                        const struct sftpattr *attrs) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_SETSTAT);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  protocol->sendattrs(&fakejob, attrs);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_SETSTAT");
  return status();
}

static int sftp_fsetstat(const struct handle *hp,
                         const struct sftpattr *attrs) {
  uint32_t id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_FSETSTAT);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_bytes(&fakeworker, hp->data, hp->len);
  protocol->sendattrs(&fakejob, attrs);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_FSETSTAT");
  return status();
}

static int sftp_rmdir(const char *path) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_RMDIR);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_RMDIR");
  return status();
}

static int sftp_remove(const char *path) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_REMOVE);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_REMOVE");
  return status();
}

static int sftp_rename(const char *oldpath, const char *newpath, 
                       unsigned flags) {
  uint32_t id;
  
  remote_cwd();
  /* In v3/4 atomic is assumed, overwrite and native are not available */
  if(protocol->version <= 4 && (flags & ~SSH_FXF_RENAME_ATOMIC) != 0)
    return error("cannot emulate rename flags %#x in protocol %d",
                 flags, protocol->version);
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_RENAME);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(oldpath));
  sftp_send_path(&fakejob, &fakeworker, makeabspath(newpath));
  if(protocol->version >= 5)
    sftp_send_uint32(&fakeworker, flags);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_RENAME");
  return status();
}

static int sftp_prename(const char *oldpath, const char *newpath) {
  uint32_t id;
  
  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_EXTENDED);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_string(&fakeworker, "posix-rename@openssh.org");
  sftp_send_path(&fakejob, &fakeworker, makeabspath(oldpath));
  sftp_send_path(&fakejob, &fakeworker, makeabspath(newpath));
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "posix-rename@openssh.org");
  return status();
}

static int sftp_link(const char *targetpath, const char *linkpath,
                     int sftp_send_symlink) {
  uint32_t id;
  
  if(protocol->version < 6 && !sftp_send_symlink)
    return error("hard links not supported in protocol %"PRIu32,
                 protocol->version);
  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, (protocol->version >= 6
                           ? SSH_FXP_LINK
                           : SSH_FXP_SYMLINK));
  sftp_send_uint32(&fakeworker, id = newid());
  if(quirk_reverse_symlink) {
    /* OpenSSH server gets SSH_FXP_SYMLINK args back to front
     * - see http://bugzilla.mindrot.org/show_bug.cgi?id=861 */
    sftp_send_path(&fakejob, &fakeworker, targetpath);
    sftp_send_path(&fakejob, &fakeworker, makeabspath(linkpath));
  } else {
    sftp_send_path(&fakejob, &fakeworker, makeabspath(linkpath));
    sftp_send_path(&fakejob, &fakeworker, 
              sftp_send_symlink ? targetpath : makeabspath(targetpath));
  }
  if(protocol->version >= 6)
    sftp_send_uint8(&fakeworker, !!sftp_send_symlink);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_*LINK");
  return status();
}

static int sftp_open(const char *path, 
                     uint32_t desired_access, uint32_t flags,
                     const struct sftpattr *attrs,
                     struct handle *hp) {
  uint32_t id, pflags = 0;

  remote_cwd();
  if(protocol->version <= 4) {
    /* We must translate the v5/6 style parameters back down to v3/4 */
    if(desired_access & ACE4_READ_DATA)
      pflags |= SSH_FXF_READ;
    if(desired_access & ACE4_WRITE_DATA)
      pflags |= SSH_FXF_WRITE;
    switch(flags & SSH_FXF_ACCESS_DISPOSITION) {
    case SSH_FXF_CREATE_NEW:
      pflags |= SSH_FXF_CREAT|SSH_FXF_EXCL;
      break;
    case SSH_FXF_CREATE_TRUNCATE:
      pflags |= SSH_FXF_CREAT|SSH_FXF_TRUNC;
      break;
    case SSH_FXF_OPEN_OR_CREATE:
      pflags |= SSH_FXF_CREAT;
      break;
    case SSH_FXF_OPEN_EXISTING:
      break;
    case SSH_FXF_TRUNCATE_EXISTING:
      pflags |= SSH_FXF_TRUNC;
      break;
    default:
      return error("unknown SSH_FXF_ACCESS_DISPOSITION %#"PRIx32,
                   flags);
    }
    if(flags & (SSH_FXF_APPEND_DATA|SSH_FXF_APPEND_DATA_ATOMIC))
      pflags |= SSH_FXF_APPEND;
    if(flags & SSH_FXF_TEXT_MODE) {
      if(protocol->version < 4)
        return error("SSH_FXF_TEXT_MODE cannot be emulated in protocol %d",
                     protocol->version);
      else
        pflags |= SSH_FXF_TEXT;
    }
    if(flags & ~(SSH_FXF_ACCESS_DISPOSITION
                 |SSH_FXF_APPEND_DATA
                 |SSH_FXF_APPEND_DATA_ATOMIC
                 |SSH_FXF_TEXT_MODE)) 
      return error("future SSH_FXP_OPEN flags (%#"PRIx32") cannot be emulated in protocol %d",
                   flags, protocol->version);
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_OPEN);
    sftp_send_uint32(&fakeworker, id = newid());
    sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
    sftp_send_uint32(&fakeworker, pflags);
    protocol->sendattrs(&fakejob, attrs);
    sftp_send_end(&fakeworker);
  } else {
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_OPEN);
    sftp_send_uint32(&fakeworker, id = newid());
    sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
    sftp_send_uint32(&fakeworker, desired_access);
    sftp_send_uint32(&fakeworker, flags);
    protocol->sendattrs(&fakejob, attrs);
    sftp_send_end(&fakeworker);
  }
  if(getresponse(SSH_FXP_HANDLE, id, "SSH_FXP_OPEN") != SSH_FXP_HANDLE)
    return -1;
  cpcheck(sftp_parse_string(&fakejob, &hp->data, &hp->len));
  return 0;
}

struct space_available {
  uint64_t bytes_on_device;
  uint64_t unused_bytes_on_device;
  uint64_t bytes_available_to_user;
  uint64_t unused_bytes_available_to_user;
  uint32_t bytes_per_allocation_unit;
};

static int sftp_space_available(const char *path,
                                struct space_available *as) {
  uint32_t id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_EXTENDED);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_string(&fakeworker, "space-available");
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_EXTENDED_REPLY,
                 id, "space-available") != SSH_FXP_EXTENDED_REPLY)
    return -1;
  cpcheck(sftp_parse_uint64(&fakejob, &as->bytes_on_device));
  cpcheck(sftp_parse_uint64(&fakejob, &as->unused_bytes_on_device));
  cpcheck(sftp_parse_uint64(&fakejob, &as->bytes_available_to_user));
  cpcheck(sftp_parse_uint64(&fakejob, &as->unused_bytes_available_to_user));
  cpcheck(sftp_parse_uint32(&fakejob, &as->bytes_per_allocation_unit));
  return 0;
}

static int sftp_mkdir(const char *path, mode_t mode) {
  struct sftpattr attrs;
  uint32_t id;

  remote_cwd();
  if(mode == (mode_t)-1)
    attrs.valid = 0;
  else {
    attrs.valid = SSH_FILEXFER_ATTR_PERMISSIONS;
    attrs.permissions = mode;
  }
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_MKDIR);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  protocol->sendattrs(&fakejob, &attrs);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "SSH_FXP_MKDIR");
  return status();
}

static char *sftp_readlink(const char *path) {
  char *resolved;
  uint32_t u32, id;

  remote_cwd();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READLINK);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_path(&fakejob, &fakeworker, makeabspath(path));
  sftp_send_end(&fakeworker);
  if(getresponse(SSH_FXP_NAME, id, "SSH_FXP_READLINK") != SSH_FXP_NAME)
    return 0;
  cpcheck(sftp_parse_uint32(&fakejob, &u32));
  if(u32 != 1)
    fatal("wrong count in SSH_FXP_READLINK reply");
  cpcheck(sftp_parse_path(&fakejob, &resolved));
  return resolved;
}

static int sftp_text_seek(const struct handle *hp, uint64_t line) {
  uint32_t id;

  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_EXTENDED);
  sftp_send_uint32(&fakeworker, id = newid());
  sftp_send_string(&fakeworker, "text-seek");
  sftp_send_bytes(&fakeworker, hp->data, hp->len);
  sftp_send_uint64(&fakeworker, line);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id, "text-seek");
  return status();
}

/* Command line operations */

static int cmd_pwd(int attribute((unused)) ac,
                   char attribute((unused)) **av) {
  xprintf("%s\n", remote_cwd());
  return 0;
}

static int cmd_cd(int attribute((unused)) ac,
                  char **av) {
  const char *newcwd;
  struct sftpattr attrs;

  remote_cwd();
  if(protocol->version >= 6) {
    /* We can do this more efficiently */
    newcwd = sftp_realpath_v6(cwd, SSH_FXP_REALPATH_STAT_ALWAYS, av, &attrs);
    if(!newcwd)
      return -1;
  } else {
    newcwd = sftp_realpath(makeabspath(av[0]));
    if(!newcwd)
      return -1;
    /* Check it's really a directory */
    if(sftp_stat(newcwd, &attrs, SSH_FXP_LSTAT))
      return -1;
  }
  if(attrs.type != SSH_FILEXFER_TYPE_DIRECTORY) {
    error("%s is not a directory (type %d)", av[0], attrs.type);
    return -1;
  }
  free(cwd);
  cwd = xstrdup(newcwd);
  return 0;
}

static int cmd_quit(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  exit(0);
}

static int cmd_help(int attribute((unused)) ac,
                    char attribute((unused)) **av);

static int cmd_lpwd(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  char *lpwd;

  if(!(lpwd = sftp_getcwd(fakejob.a))) {
    error("error calling getcwd: %s", strerror(errno));
    return -1;
  }
  xprintf("%s\n", lpwd);
  return 0;
}

static int cmd_lcd(int attribute((unused)) ac,
                   char **av) {
  if(chdir(av[0]) < 0) {
    error("error calling chdir: %s", strerror(errno));
    return -1;
  }
  return 0;
}

static int sort_by_name(const void *av, const void *bv) {
  const struct sftpattr *const a = av, *const b = bv;

  return strcmp(a->name, b->name);
}

static int sort_by_size(const void *av, const void *bv) {
  const struct sftpattr *const a = av, *const b = bv;

  if(a->valid & b->valid & SSH_FILEXFER_ATTR_SIZE) {
    if(a->size < b->size)
      return -1;
    else if(a->size > b->size)
      return 1;
  }
  return sort_by_name(av, bv);
}

static int sort_by_mtime(const void *av, const void *bv) {
  const struct sftpattr *const a = av, *const b = bv;

  if(a->valid & b->valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    if(a->mtime.seconds < b->mtime.seconds)
      return -1;
    else if(a->mtime.seconds > b->mtime.seconds)
      return 1;
    if(a->valid & b->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {
      if(a->mtime.nanoseconds < b->mtime.nanoseconds)
        return -1;
      else if(a->mtime.nanoseconds > b->mtime.nanoseconds)
        return 1;
    }
  }
  return sort_by_name(av, bv);
}

/* Reverse an array in place */
static void reverse(void *array, size_t count, size_t size) {
  void *tmp = xmalloc(size);
  char *const base = array;
  size_t n;
  
  /* If size is even then size/2 is the first half of the array and that's fine.
   * If size is odd then size/2 goes up to but excludes the middle member
   * which is also fine. */
  for(n = 0; n < size / 2; ++n) {
    memcpy(tmp, base + n * size, size);
    memcpy(base + n * size, base + (count - n - 1) * size, size);
    memcpy(base + (count - n - 1) * size, tmp, size);
  }
  free(tmp);
}

static int cmd_ls(int ac,
                  char **av) {
  const char *options, *path;
  struct sftpattr *attrs, *allattrs = 0, fileattrs;
  size_t nattrs, nallattrs = 0, n, m, i, maxnamewidth = 0;
  struct handle h;
  size_t cols, rows;
  int singlefile;

  remote_cwd();
  if(ac > 0 && av[0][0] == '-') {
    options = *av++;
    --ac;
  } else
    options = "";
  path = ac > 0 ? av[0] : remote_cwd();
  /* See what type the file is */
  if(sftp_stat(path, &fileattrs, SSH_FXP_LSTAT))
    return -1;
  if(fileattrs.type != SSH_FILEXFER_TYPE_DIRECTORY
     || strchr(options, 'd')) {
    /* The file is not a directory, or we used -d */
    allattrs = &fileattrs;
    nallattrs = 1;
    singlefile = 1;
  } else {
    const int include_dotfiles = !!strchr(options, 'a');

    /* The file is a directory and we did not use -d */
    singlefile = 0;
    if(sftp_opendir(path, &h))
      return -1;
    for(;;) {
      if(sftp_readdir(&h, &attrs, &nattrs)) {
        sftp_close(&h);
        free(allattrs);
        return -1;
      }
      if(!nattrs)
        break;                          /* eof */
      allattrs = xrecalloc(allattrs, nattrs + nallattrs, sizeof *attrs);
      for(n = 0; n < nattrs; ++n) {
        const size_t w = wcswidth(attrs[n].wname, SIZE_MAX);
        if(include_dotfiles || attrs[n].name[0] != '.') {
          if(w > maxnamewidth)
            maxnamewidth = w;
          allattrs[nallattrs++] = attrs[n];
        }
      }
    }
    sftp_close(&h);
  }
  if(!strchr(options, 'f')) {
    int (*sorter)(const void *, const void *);

    if(strchr(options, 'S'))
      sorter = sort_by_size;
    else if(strchr(options, 't'))
      sorter = sort_by_mtime;
    else
      sorter = sort_by_name;
    qsort(allattrs, nallattrs, sizeof *allattrs, sorter);
    if(strchr(options, 'r'))
      reverse(allattrs, nallattrs, sizeof *allattrs);
  }
  if(strchr(options, 'l') || strchr(options, 'n')) {
    /* long listing */
    time_t now;
    struct tm nowtime;
    const unsigned long flags = (strchr(options, 'n')
                                 ? FORMAT_PREFER_NUMERIC_UID
                                 : 0)|FORMAT_PREFER_LOCALTIME|FORMAT_ATTRS;
    
    /* We'd like to know what year we're in for dates in longname */
    time(&now);
    gmtime_r(&now, &nowtime);
    for(n = 0; n < nallattrs; ++n) {
      struct sftpattr *const attrs = &allattrs[n];
      if(attrs->type == SSH_FILEXFER_TYPE_SYMLINK
         && !attrs->target) {
        if(singlefile)
          attrs->target = sftp_readlink(attrs->name);
        else {
          char *const fullname = sftp_alloc(fakejob.a,
                                            strlen(path) + strlen(attrs->name)
                                            + 2);
          strcpy(fullname, path);
          strcat(fullname, "/");
          strcat(fullname, attrs->name);
          D(("%s -> %s",  attrs->name, fullname));
          attrs->target = sftp_readlink(fullname);
        }
      }
      xprintf("%s\n", sftp_format_attr(fakejob.a, attrs, nowtime.tm_year, flags));
    }
  } else if(strchr(options, '1')) {
    /* single-column listing */
    for(n = 0; n < nallattrs; ++n)
      xprintf("%s\n", allattrs[n].name);
  } else {
    /* multi-column listing.  First figure out the terminal width. */
    /* We have C columns of width M, with C-1 single-character blank columns
     * between them.  So the total width is C * M + (C - 1).  The lot had
     * better fit into W columns in total.
     * 
     * We want to find the maximum C such that:
     *     C * M + C - 1 <= W
     * <=> C * (M + 1) <= W + 1
     * <=> C <= (W + 1) / (M + 1)
     *
     * Therefore we calculate C=floor[(W + 1) / (M + 1)].
     *
     * For instance W=80 and M=40 gives 81/41 = 1
     *              W=80 and M=39 gives 81/39 = 2
     *              W=80 and M=27 gives 81/28 = 2
     *              W=80 and M=26 gives 81/27 = 3
     * and so on.
     *
     * If we end up with 0 columns we round it up to 1 and put up with the
     * overflow.
     */
    cols = (terminal_width + 1) / (maxnamewidth + 1);
    if(!cols)
      cols = 1;
    /* Now we have the number of columns.  The N files are conventionally
     * listed down the columns, in this sort of order:
     *
     *    0  4  8
     *    1  5  9
     *    2  6  10
     *    3  7  11
     *
     * Up to the last C-1 columns may be missing their final row.  So there
     * must be ceil(N / C) rows.  We calculate this slightly indirectly.
     */
    rows = (nallattrs + cols - 1) / cols;
    for(n = 0; n < rows; ++n) {
      for(m = 0; m < cols && (i = n + m * rows) < nallattrs; ++m) {
        const size_t w = wcswidth(allattrs[i].wname, SIZE_MAX);

        xprintf("%s%*s",
                allattrs[i].name,
                (m + 1 < cols && i + rows < nallattrs
                 ? (int)(maxnamewidth - w + 1) : 0), "");
      }
      xprintf("\n");
    }
  }
  if(allattrs != &fileattrs)
    free(allattrs);
  return 0;
}

static int cmd_lls(int ac,
                   char **av) {
  const char **args = sftp_alloc(fakejob.a, (ac + 2) * sizeof (char *));
  int n = 0;
  pid_t pid;

  args[n++] = "ls";
  while(ac--)
    args[n++] = *av++;
  args[n] = 0;
  if(!(pid = xfork())) {
    execvp(args[0], (void *)args);
    fatal("executing ls: %s", strerror(errno));
  }
  if(waitpid(pid, &n, 0) < 0) {
    fatal("error calling waitpid: %s", strerror(errno));
    if(n) {
      error("ls returned status %#x", n);
      return -1;
    }
  }
  return 0;
}

static int cmd_lumask(int ac,
                      char **av) {
  mode_t n;

  if(ac) {
    errno = 0;
    n = strtol(av[0], 0, 8);
    if(errno) {
      error("invalid umask: %s", strerror(errno));
      return -1;
    }
    if(n != (n & 0777)) {
      error("umask out of range");
      return -1;
    }
    umask(n);
  } else {
    n = umask(0);
    umask(n);
    xprintf("%03o\n", (unsigned)n);
  }
  return 0;
}

static int cmd_lmkdir(int attribute((unused)) ac,
                      char **av) {
  if(mkdir(av[0], 0777) < 0) {
    error("creating directory %s: %s", av[0], strerror(errno));
    return -1;
  }
  return 0;
}

static int cmd_chown(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  
  if(sftp_stat(av[1], &attrs, SSH_FXP_STAT))
    return -1;
  if(protocol->version >= 4) {
    if(!(attrs.valid & SSH_FILEXFER_ATTR_OWNERGROUP))
      return error("cannot determine former owner/group");
    attrs.owner = av[0];
  } else {
    if(!(attrs.valid & SSH_FILEXFER_ATTR_UIDGID))
      return error("cannot determine former UID/GID");
    attrs.uid = atoi(av[0]);
  }
  return sftp_setstat(av[1], &attrs);
}

static int cmd_chgrp(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  
  if(sftp_stat(av[1], &attrs, SSH_FXP_STAT))
    return -1;
  if(protocol->version >= 4) {
    if(!(attrs.valid & SSH_FILEXFER_ATTR_OWNERGROUP))
      return error("cannot determine former owner/group");
    attrs.group = av[0];
  } else {
    if(!(attrs.valid & SSH_FILEXFER_ATTR_UIDGID))
      return error("cannot determine former UID/GID");
    attrs.gid = atoi(av[0]);
  }
  return sftp_setstat(av[1], &attrs);
}

static int cmd_chmod(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  
  attrs.valid = SSH_FILEXFER_ATTR_PERMISSIONS;
  errno = 0;
  attrs.permissions = strtol(av[0], 0, 8);
  if(errno || attrs.permissions != (attrs.permissions & 07777))
    error("invalid permissions: %s", strerror(errno));
  return sftp_setstat(av[1], &attrs);
}

static int cmd_rm(int attribute((unused)) ac,
                  char **av) {
  return sftp_remove(av[0]);
}

static int cmd_rmdir(int attribute((unused)) ac,
                     char **av) {
  return sftp_rmdir(av[0]);
}

static int cmd_mv(int attribute((unused)) ac,
                     char **av) {
  if(ac == 3) {
    const char *ptr = av[0];
    int c;
    unsigned flags = 0;
    int posixrename = 0;

    if(*ptr++ != '-')
      return error("invalid options '%s'", av[0]);
    while((c = *ptr++)) {
      switch(c) {
      case 'n': flags |= SSH_FXF_RENAME_NATIVE; break;
      case 'a': flags |= SSH_FXF_RENAME_ATOMIC; break;
      case 'o': flags |= SSH_FXF_RENAME_OVERWRITE; break;
      case 'p': posixrename = 1; break;
      default: return error("invalid options '%s'", av[0]);
      }
    }
    if(posixrename)
      return sftp_prename(av[1], av[2]);
    else
      return sftp_rename(av[1], av[2], flags);
  } else
    return sftp_rename(av[0], av[1], 0);
}

static int cmd_symlink(int attribute((unused)) ac,
                       char **av) {
  return sftp_link(av[0], av[1], 1);
}

static int cmd_link(int attribute((unused)) ac,
                    char **av) {
  return sftp_link(av[0], av[1], 0);
}

/* cmd_get uses a background thread to send requests */
struct outstanding_read {
  uint32_t id;                          /* 0 or a request ID */
  off_t offset;                         /* offset in source file */
};

struct reader_data {
  pthread_mutex_t m;                    /* protects everything here */
  pthread_cond_t c1;                    /* signaled when a response received */
  pthread_cond_t c2;                    /* signaled when a request sent */
  struct handle h;                      /* target handle */
  struct outstanding_read *reqs;        /* in-flight requests */
  uint64_t next_offset;                 /* next offset */
  int outstanding, eof, failed;
  uint64_t size;                        /* file size */
  uint64_t written;                     /* byte written so far */
  int fd;                               /* file to write to */
  char *local, *tmp;                    /* output filename */
  /* For text translation: */
  FILE *translated_fp;                  /* file to write to (in stdio-speak) */
  size_t translated_state;              /* how far thru newline we've seen */
};

static void *reader_thread(void *arg) {
  struct reader_data *const r = arg;
  int n;
  uint32_t id, len;
  
  ferrcheck(pthread_mutex_lock(&r->m));
  while(!r->eof && !r->failed) {
    /* Wait for a job to be reaped */
    while(r->outstanding == nrequests && !r->eof)
      ferrcheck(pthread_cond_wait(&r->c1, &r->m));
    /* Send as many jobs as we can */
    while(r->outstanding < nrequests && !r->eof) {
      /* Find a spare slot */
      for(n = 0; n < nrequests && r->reqs[n].id; ++n)
        ;
      assert(n < nrequests);
      id = newid();
      sftp_send_begin(&fakeworker);
      sftp_send_uint8(&fakeworker, SSH_FXP_READ);
      sftp_send_uint32(&fakeworker, id);
      sftp_send_bytes(&fakeworker, r->h.data, r->h.len);
      sftp_send_uint64(&fakeworker, r->next_offset);
      if(r->size == (uint64_t)-1 || r->size - r->next_offset > buffersize)
        len = buffersize;
      else {
        len = (uint32_t)(r->size - r->next_offset);
        r->eof = 1;
      }
      sftp_send_uint32(&fakeworker, len);
      /* Fill in the outstanding_read once we've sent it */
      r->reqs[n].id = id;
      r->reqs[n].offset = r->next_offset;
      ++r->outstanding;
      r->next_offset += buffersize;
      /* Don't hold the lock while doing the send itself */
      ferrcheck(pthread_mutex_unlock(&r->m));
      sftp_send_end(&fakeworker);
      ferrcheck(pthread_mutex_lock(&r->m));
      /* Notify the main threader that we set off a request */
      ferrcheck(pthread_cond_signal(&r->c2));
    }
  }
  ferrcheck(pthread_mutex_unlock(&r->m));
  return 0;
}

/* Inbound text file translation */

static int write_translated_init(struct reader_data *r) {
  if(!(r->translated_fp = fdopen(r->fd, "w")))
    return error("error calling fdopen: %s", strerror(errno));
  r->translated_state = 0;
  return 0;
}

static int write_translated(struct reader_data *r,
                            const void *vptr, size_t bytes) {
  const char *ptr = vptr;
  while(bytes > 0) {
    const int c = (unsigned char)*ptr++;
    --bytes;
    if(c == newline[r->translated_state]) {
      /* We've seen part of a newline */
      ++r->translated_state;
      if(!newline[r->translated_state]) {
        /* We must have seen a whole newline */
        if(putc('\n', r->translated_fp) < 0)
          return -1;
        r->translated_state = 0;
      } else {
        /* We're part way thru something that might be a newline.  Keep
         * going. */
      }
    } else {
      if(r->translated_state) {
        /* We're part way thru something that turned out not to be a newline. */
        if(fwrite(newline, 1, r->translated_state, r->translated_fp)
           != r->translated_state)
          return -1;
        r->translated_state = 0;
        /* Try again from the current point. */
        /* Note that we assume that the newline sequence doesn't contain
         * repetitions.  If you have a (completely bonkers!) platform that
         * violates this assumption then you'll have to write a cleverer state
         * machine here. */
        continue;
      } else {
        if(putc(c, r->translated_fp) < 0)
          return -1;
      }
    }
  }
  return 0;
}

static int write_translated_done(struct reader_data *r) {
  int rc = 0;

  if(r->translated_fp) {
    if(r->translated_state) {
      /* The file ends part way thru something that starts out like a newline
       * sequence but turns out not to be one. */
      if(fwrite(newline, 1, r->translated_state, r->translated_fp) != r->translated_state)
        rc = -1;
      r->translated_state = 0;
    }
    if(fclose(r->translated_fp) < 0)
      rc = -1;
    r->translated_fp = 0;
  }
  return rc;
}

static void reap_write_response(struct reader_data *r) {
  uint8_t rtype;
  uint32_t st, len;
  int n, rc;

  /* Get the next response and count it down.  We don't hold the lock while
   * were awaiting the response. */
  ferrcheck(pthread_mutex_unlock(&r->m));
  rtype = getresponse(-1, 0, "SSH_FXP_READ");
  ferrcheck(pthread_mutex_lock(&r->m));
  --r->outstanding;
  /* If we've already failed then we don't care what the response was */
  if(r->failed)
    return;
  switch(rtype) {
  case SSH_FXP_STATUS:
    cpcheck(sftp_parse_uint32(&fakejob, &st));
    if(st == SSH_FX_EOF)
      r->eof = 1;
    else {
      status();
      r->failed = 1;
    }
    break;
  case SSH_FXP_DATA:
    /* Find the right request */
    for(n = 0; n < nrequests && fakejob.id != r->reqs[n].id; ++n)
      ;
    assert(n < nrequests);
    /* Free up this slot */
    r->reqs[n].id = 0;
    /* We don't fully parse the string but instead write it out from the
     * input buffer it's sitting in. */
    cpcheck(sftp_parse_uint32(&fakejob, &len));
    /* We don't care what order the responses come in, we just write them to
     * the right place in the file according to what we asked for.  There's
     * not much point releasing the lock while doing the write - the
     * background thread will have done all it wants while we were awaiting
     * the response. 
     *
     * In text mode we can rely on the responses arriving in the correct
     * order.
     */
    if(textmode) {
      /* We must replace each instance of the newline string with \n */
      rc = write_translated(r, fakejob.ptr, len);
    } else
      rc = pwrite(r->fd, fakejob.ptr, len, r->reqs[n].offset);
    if(rc < 0) {
      error("error writing to %s: %s", r->tmp, strerror(errno));
      r->failed = 1;
      return;
    }
    r->written += len;
    progress(r->local, r->written, r->size);
    break;
  default:
    fatal("unexpected response %d to SSH_FXP_READ", rtype);
  }
}

static int cmd_get(int ac,
                   char **av) {
  int preserve = 0;
  const char *e;
  char *remote;
  struct reader_data r;
  struct sftpattr attrs;
  pthread_t tid;
  struct timeval started, finished;
  double elapsed;
  uint32_t flags = 0;
  int seek = 0;
  uint64_t line = 0;

  memset(&attrs, 0, sizeof attrs);
  memset(&r, 0, sizeof r);
  r.fd = -1;
  if(av[0][0] == '-') {
    const char *s = *av++;
    ++s;
    while(*s) {
      switch(*s++) {
      case 'P':
        preserve = 1;
        break;
      case 'f':
        flags |= SSH_FXF_NOFOLLOW; 
        break;
      case 'L':
        seek = 1;
        line = (uint64_t)strtoull(s, 0, 10);
        s = "";
        break;
      default:
        return error("unknown get option -%c'", s[-1]);
      }
    }
    --ac;
  }
  remote = *av++;
  --ac;
  if(ac) {
    r.local = *av++;
    --ac;
  } else
    r.local = basename(remote);
  /* we'll write to a temporary file */
  r.tmp = sftp_alloc(fakejob.a, strlen(r.local) + 5);
  sprintf(r.tmp, "%s.new", r.local);
  if((r.fd = open(r.tmp, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0) {
    error("error opening %s: %s", r.tmp, strerror(errno));
    goto error;
  }
  if(textmode) {
    flags |= SSH_FXF_TEXT_MODE;
    if(write_translated_init(&r))
      goto error;
    r.fd = -1;
  }
  /* open the remote file */
  if(sftp_open(remote, ACE4_READ_DATA|ACE4_READ_ATTRIBUTES,
               SSH_FXF_OPEN_EXISTING|flags,
               &attrs, &r.h))
    goto error;
  /* stat the file */
  if(sftp_fstat(&r.h, &attrs))
    goto error;
  if(!textmode && (attrs.valid & SSH_FILEXFER_ATTR_SIZE)) {
    /* We know how big the file is.  Check we can fit it! */
    if((uint64_t)(off_t)attrs.size != attrs.size) {
      error("remote file %s is too large (%"PRIu64" bytes)",
            remote, attrs.size);
      goto error;
    }
    r.size = attrs.size;
  } else
    /* We don't know how big the file is.  We'll just keep on reading until we
     * get an EOF.  This includes text files where translation may make the
     * size read back with fstat a lie. */
    r.size = (uint64_t)-1;
  if(textmode && seek) {
    if(sftp_text_seek(&r.h, line))
      goto error;
  }
  gettimeofday(&started,  0);
  ferrcheck(pthread_mutex_init(&r.m, 0));
  ferrcheck(pthread_cond_init(&r.c1, 0));
  ferrcheck(pthread_cond_init(&r.c2, 0));
  r.reqs = sftp_alloc(fakejob.a, nrequests * sizeof *r.reqs);
  ferrcheck(pthread_create(&tid, 0, reader_thread, &r));
  ferrcheck(pthread_mutex_lock(&r.m));
  /* If there are requests in flight, we must keep going whatever else
   * is happening in order to process their replies.
   * If there are no requests in flight then we keep going until
   * we reach EOF or an error is detected.
   */
  while(!r.eof && !r.failed) {
    /* Wait until there's at least one request in flight */
    while(!r.outstanding)
      ferrcheck(pthread_cond_wait(&r.c2, &r.m));
    reap_write_response(&r);
    /* Notify the background thread that something changed. */ 
    ferrcheck(pthread_cond_signal(&r.c1));
  }
  ferrcheck(pthread_mutex_unlock(&r.m));
  /* Wait for the background thread to finish up */
  ferrcheck(pthread_join(tid, 0));
  /* Reap any remaining jobs */
  ferrcheck(pthread_mutex_lock(&r.m));
  while(r.outstanding)
    reap_write_response(&r);
  ferrcheck(pthread_mutex_unlock(&r.m));
  /* Tear all the thread objects down */
  ferrcheck(pthread_mutex_destroy(&r.m));
  ferrcheck(pthread_cond_destroy(&r.c1));
  ferrcheck(pthread_cond_destroy(&r.c2));
  progress(0, 0, 0);
  if(r.failed) 
    goto error;
  gettimeofday(&finished, 0);
  if(progress_indicators) {
    elapsed = ((finished.tv_sec - started.tv_sec)
               + (finished.tv_usec - started.tv_usec) / 1000000.0);
    xprintf("%"PRIu64" bytes in %.1f seconds", r.written, elapsed);
    if(elapsed > 0.1)
      xprintf(" %.0f bytes/sec", r.written / elapsed);
    xprintf("\n");
  }
  /* Close the handle */
  sftp_close(&r.h);
  r.h.len = 0;
  if(preserve) {
    /* Set permissions etc */
    attrs.valid &= ~SSH_FILEXFER_ATTR_SIZE; /* don't truncate */
    attrs.valid &= ~SSH_FILEXFER_ATTR_UIDGID; /* different mapping! */
    if(sftp_set_fstatus(fakejob.a, r.fd, &attrs, &e) != SSH_FX_OK) {
      error("cannot %s %s: %s", e, r.tmp, strerror(errno));
      goto error;
    }
  }
  if(textmode) {
    if(write_translated_done(&r)) {
      error("error writing to %s: %s", r.tmp, strerror(errno));
      goto error;
    }
  } else if(close(r.fd) < 0) {
    error("error closing %s: %s", r.tmp, strerror(errno));
    r.fd = -1;
    goto error;
  }
  if(rename(r.tmp, r.local) < 0) {
    error("error renaming %s: %s", r.tmp, strerror(errno));
    goto error;
  }
  return 0;
error:
  write_translated_done(&r);            /* ok to call if not initialized */
  if(r.fd >= 0)
    close(r.fd);
  if(r.tmp)
    unlink(r.tmp);
  if(r.h.len)
    sftp_close(&r.h);
  return -1;
}

/* put uses a thread to gather responses */
struct outstanding_write {
  uint32_t id;                          /* or 0 for empty slot */
  ssize_t n;                            /* size of this request */
};

struct writer_data {
  pthread_mutex_t m;                    /* protects everything here */
  pthread_cond_t c1;                    /* signal when writer modifies */
  pthread_cond_t c2;                    /* signal when reader modifies */
  int failed;                           /* set on failure */
  int outstanding;                      /* number of outstanding requests */
  int finished;                         /* set when writer finished */
  struct outstanding_write *reqs;       /* outstanding requests */
  const char *remote;                   /* remote path */
  uint64_t written, total;              /* total size */
};

static void *writer_thread(void *arg) {
  struct writer_data *const w = arg;
  int i;
  uint32_t st;

  ferrcheck(pthread_mutex_lock(&w->m));
  /* Keep going until the writer has finished and there are no outstanding
   * requests left */
  while(!w->finished || w->outstanding) {
    /* Wait until there's at least one request outstanding */
    if(!w->outstanding) {
      ferrcheck(pthread_cond_wait(&w->c1, &w->m));
      continue;
    }
    /* Await the incoming response */
    getresponse(SSH_FXP_STATUS, 0/*don't care about id*/, "SSH_FXP_WRITE");
    ferrcheck(pthread_cond_signal(&w->c2));
    /* Find the request ID */
    for(i = 0; i < nrequests && w->reqs[i].id != fakejob.id; ++i)
      ;
    assert(i < nrequests);
    --w->outstanding;
    cpcheck(sftp_parse_uint32(&fakejob, &st));
    if(st == SSH_FX_OK) {
      w->written += w->reqs[i].n;
      w->reqs[i].id = 0;
      progress(w->remote, w->written, w->total);
    } else if(!w->failed) {
      /* Only report the first error */
      status();
      w->failed = 1;
    }
  }
  progress(0, 0, 0);                    /* clear progress indicator */
  ferrcheck(pthread_mutex_unlock(&w->m));
  return 0;
}

static int cmd_put(int ac,
                   char **av) {
  char *local;
  const char *remote;
  struct sftpattr attrs;
  struct stat sb;
  int fd = -1, i, preserve = 0, failed = 0, eof = 0;
  struct handle h;
  off_t offset;
  ssize_t n;
  struct writer_data w;
  pthread_t tid;
  struct timeval started, finished;
  double elapsed;
  uint32_t id;
  FILE *fp = 0;
  uint32_t disp = SSH_FXF_CREATE_TRUNCATE, flags = 0;
  int setmode = 0;
  mode_t mode = 0;

  memset(&h, 0, sizeof h);
  memset(&attrs, 0, sizeof attrs);
  memset(&w, 0, sizeof w);
  if(av[0][0] == '-') {
    const char *s = *av++;
    ++s;
    while(*s) {
      switch(*s++) {
      case 'P':
        preserve = 1;
        break;
      case 'a': 
        disp = SSH_FXF_OPEN_OR_CREATE;
        flags |= SSH_FXF_APPEND_DATA; 
        break;
      case 'A': 
        disp = SSH_FXF_OPEN_EXISTING;
        flags |= SSH_FXF_APPEND_DATA; 
        break;
      case 'f':
        flags |= SSH_FXF_NOFOLLOW; 
        break;
      case 't':
        disp = SSH_FXF_CREATE_NEW;
        break;
      case 'e':
        disp = SSH_FXF_TRUNCATE_EXISTING;
        break;
      case 'd':
        flags |= SSH_FXF_DELETE_ON_CLOSE;
        break;
      case 'm':
        setmode = 1;
        mode = strtoul(s, 0, 8);
        s = "";
        break;
      default:
        return error("unknown put option -%c'", s[-1]);
      }
    }
    --ac;
  }
  local = *av++;
  --ac;
  if(ac) {
    remote = *av++;
    --ac;
  } else
    remote = basename(local);
 if((fd = open(local, O_RDONLY)) < 0) {
    error("cannot open %s: %s", local, strerror(errno));
    goto error;
  }
  if(fstat(fd, &sb) < 0) {
    error("cannot stat %s: %s", local, strerror(errno));
    goto error;
  }
  if(S_ISDIR(sb.st_mode)) {
    error("%s is a directory", local);
    goto error;
  }
  if(S_ISREG(sb.st_mode)) {
    w.total = (uint64_t)sb.st_size;
    if((off_t)w.total != sb.st_size) {
      error("%s is too large to upload via SFTP", local);
      goto error;
    }
  } else
    w.total = (uint64_t)-1;
  if(preserve) {
    sftp_stat_to_attrs(fakejob.a, &sb, &attrs, 0xFFFFFFFF, local);
    /* Mask out things that don't make sense: we set the size by dint of
     * uploading data, we don't want to try to set a numeric UID or GID, and we
     * cannot set the allocation size or link count. */
    attrs.valid &= ~(SSH_FILEXFER_ATTR_SIZE
                     |SSH_FILEXFER_ATTR_LINK_COUNT
                     |SSH_FILEXFER_ATTR_UIDGID);
    /* Mask off attributes that don't work in this protocol version */
    attrs.valid &= attrmask;
    assert(!(attrs.valid & SSH_FILEXFER_ATTR_CTIME));
    attrs.attrib_bits &= ~SSH_FILEXFER_ATTR_FLAGS_HIDDEN;
  }
  if(setmode) {
    attrs.valid |= SSH_FILEXFER_ATTR_PERMISSIONS;
    attrs.permissions = mode;
  }
  if(textmode)
    flags |= SSH_FXF_TEXT_MODE;
  if(sftp_open(remote, ACE4_WRITE_DATA|ACE4_WRITE_ATTRIBUTES,
               disp | flags,
               &attrs, &h))
    goto error;
  if(textmode) {
    if(!(fp = fdopen(fd, "r"))) {
      error("error calling fdopen: %s", strerror(errno));
      goto error;
    }
    fd = -1;
  }
  w.reqs = sftp_alloc(fakejob.a, nrequests * sizeof *w.reqs);
  w.remote = remote;
  gettimeofday(&started,  0);
  ferrcheck(pthread_mutex_init(&w.m, 0));
  ferrcheck(pthread_cond_init(&w.c1, 0));
  ferrcheck(pthread_cond_init(&w.c2, 0));
  ferrcheck(pthread_create(&tid, 0, writer_thread, &w));
  ferrcheck(pthread_mutex_lock(&w.m));
  offset = 0;
  while(!w.failed && !eof && !failed) {
    int wrote;
    
    /* Wait until we're allowed to send another request */
    if(w.outstanding >= nrequests) {
      ferrcheck(pthread_cond_wait(&w.c2, &w.m));
      continue;
    }
    /* Release the lock while we mess around with IO */
    ferrcheck(pthread_mutex_unlock(&w.m));
    /* Construct a write command */
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_WRITE);
    sftp_send_uint32(&fakeworker, id = newid());
    sftp_send_bytes(&fakeworker, h.data, h.len);
    sftp_send_uint64(&fakeworker, offset);
    sftp_send_need(fakejob.worker, buffersize + 4);
    wrote = 0;
    if(textmode) {
      char *const start = ((char *)fakejob.worker->buffer
                           + fakejob.worker->bufused + 4);
      char *ptr = start;
      size_t left = buffersize;
      const size_t newline_len = strlen(newline);
      int c;
      /* We will need to perform a translation step.  We read from stdio into
       * our buffer expanding newlines as necessary. */
      
      while(left > 0 && (c = getc(fp)) != EOF) {
        if(c == '\n') {
          /* We found a newline.  Let us translated it to the desired wire
           * format. */
          if(left >= newline_len) {
            /* We can fit a translated newline here */
            strcpy(ptr, newline);
            ptr += newline_len;
            left -= newline_len;
          } else {
            /* Whoops, we cannot fit a translated newline in. */
            if(ungetc(c, fp) < 0)
              fatal("ungetc: %s", strerror(errno));
          }
        } else {
          /* This character is not a newline. */
          *ptr++ = c;
          --left;
        }
      }
      /* Fake up n to look like the result of read() */
      if(ferror(fp))
        n = -1;
      else
        n = ptr - start;
      /* There is always space to write at least one translated newline (see
       * main() below) so n will only be 0 if we genuinely are at EOF. */
    } else {
      /* We read straight into our output buffer */
      n = read(fd, fakejob.worker->buffer + fakejob.worker->bufused + 4,
               buffersize);
    }
    if(n > 0) {
      /* Update the reqs[] array first, so that a reply can't arrive before its
       * ID is listed */
      ferrcheck(pthread_mutex_lock(&w.m));
      for(i = 0; i < nrequests && w.reqs[i].id; ++i)
        ;
      assert(i < nrequests);
      w.reqs[i].id = id;
      w.reqs[i].n = n;
      ++w.outstanding;
      ferrcheck(pthread_mutex_unlock(&w.m));
      /* Send off the write request with however much data we read */
      sftp_send_uint32(&fakeworker, n);
      fakejob.worker->bufused += n;
      sftp_send_end(&fakeworker);
      offset += n;
    } else if(n == 0) {
      /* We reached EOF on the input file */
      eof = 1;
    } else {
      /* Error reading the input file */
      error("error reading %s: %s", local, strerror(errno));
      failed = 1;
    }
    ferrcheck(pthread_mutex_lock(&w.m));
    ferrcheck(pthread_cond_signal(&w.c1));
  }
  w.finished = 1;
  ferrcheck(pthread_cond_signal(&w.c1));
  ferrcheck(pthread_mutex_unlock(&w.m));
  ferrcheck(pthread_join(tid, 0));
  assert(w.outstanding == 0);
  ferrcheck(pthread_mutex_destroy(&w.m));
  ferrcheck(pthread_cond_destroy(&w.c1));
  ferrcheck(pthread_cond_destroy(&w.c2));
  if(failed || w.failed)
    goto error;
  gettimeofday(&finished, 0);
  if(progress_indicators) {
    elapsed = ((finished.tv_sec - started.tv_sec)
               + (finished.tv_usec - started.tv_usec) / 1000000.0);
    xprintf("%"PRIu64" bytes in %.1f seconds", w.written, elapsed);
    if(elapsed > 0.1)
      xprintf(" %.0f bytes/sec", w.written / elapsed);
    xprintf("\n");
  }
  if(fd >= 0) {
    close(fd); 
    fd = -1;
  }
  if(fp) {
    fclose(fp);
    fp = 0;
  }
  if(preserve) {
    /* mtime at least will be nadgered */
    if(sftp_fsetstat(&h, &attrs))
      goto error;
  }
  sftp_close(&h);
  return 0;
error:
  if(fp)
    fclose(fp);
  if(fd >= 0)
    close(fd);
  if(h.len) {
    sftp_close(&h);
    sftp_remove(remote);                /* tidy up our mess */
  }
  return -1;
}

static int cmd_progress(int ac, char **av) {
  if(ac) {
    if(!strcmp(av[0], "on"))
      progress_indicators = 1;
    else if(!strcmp(av[0], "off"))
      progress_indicators = 0;
    else
      return error("invalid progress option '%s'", av[0]);
  } else
    progress_indicators ^= 1;
  return 0;
}

static int cmd_text(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  if(protocol->version < 4)
    return error("text mode not supported in protocol version %d",
                 protocol->version);
  textmode = 1;
  return 0;
}

static int cmd_binary(int attribute((unused)) ac,
                      char attribute((unused)) **av) {
  textmode = 0;
  return 0;
}

static int cmd_version(int ac,
                       char attribute((unused)) **av) {
  if(ac == 1) {
    const uint32_t id = newid();

    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_EXTENDED);
    sftp_send_uint32(&fakeworker, id);
    sftp_send_string(&fakeworker, "version-select");
    sftp_send_path(&fakejob, &fakeworker, av[0]);
    sftp_send_end(&fakeworker);
    getresponse(SSH_FXP_STATUS, id, "version-select");
    if(status()) return -1;
    if(!strcmp(av[0], "3")) protocol = &sftp_v3;
    else if(!strcmp(av[0], "4")) protocol = &sftp_v4;
    else if(!strcmp(av[0], "5")) protocol = &sftp_v5;
    else if(!strcmp(av[0], "6")) protocol = &sftp_v6;
    else fatal("unknown protocol %s", av[0]);
    return 0;
  } else {
    xprintf("Protocol version: %d\n", protocol->version);
    if(servername)
      xprintf("Server vendor:    %s\n"
              "Server name:      %s\n"
              "Server version:   %s\n"
              "Server build:     %"PRIu64"\n",
              vendorname, servername, serverversion, serverbuild);
    if(serverversions)
      xprintf("Server supports:  %s\n", serverversions);
    return 0;
  }
}

static int cmd_debug(int attribute((unused)) ac,
                     char attribute((unused)) **av) {
  sftp_debugging = !sftp_debugging;
  D(("sftp_debugging enabled"));
  return 0;
}

static void report_bytes(int width, const char *what, uint64_t howmuch) {
  static const uint64_t gbyte = (uint64_t)1 << 30;
  static const uint64_t mbyte = (uint64_t)1 << 20;
  static const uint64_t kbyte = (uint64_t)1 << 10;

  if(!howmuch)
    return;
  xprintf("%s:%*s ", what, width - (int)strlen(what), "");
  if(howmuch >= 8 * gbyte)
    xprintf("%"PRIu64" Gbytes\n", howmuch / gbyte);
  else if(howmuch >= 8 * mbyte)
    xprintf("%"PRIu64" Mbytes\n", howmuch / mbyte);
  else if(howmuch >= 8 * kbyte)
    xprintf("%"PRIu64" Kbytes\n", howmuch / kbyte);
  else 
    xprintf("%"PRIu64" bytes\n", howmuch);
}

static int cmd_df(int ac,
                  char **av) {
  struct space_available as;

  if(sftp_space_available(ac ? av[0] : remote_cwd(), &as)) 
    return -1;
  report_bytes(32, "Bytes on device", as.bytes_on_device);
  report_bytes(32, "Unused bytes on device", as.unused_bytes_on_device);
  report_bytes(32, "Available bytes on device", as.bytes_available_to_user);
  report_bytes(32, "Unused available bytes on device", 
               as.unused_bytes_available_to_user);
  report_bytes(32, "Bytes per allocation unit", as.bytes_per_allocation_unit);
  return 0;
}

static int cmd_mkdir(int ac,
                     char **av) {
  const char *path;
  mode_t mode;

  if(ac == 2) {
    mode = strtoul(av[0], 0, 8);
    path = av[1];
  } else {
    mode = -1;
    path = av[0];
  }
  return sftp_mkdir(path, mode);
}

static int cmd_init(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  return sftp_init();
}

static int cmd_unsupported(int attribute((unused)) ac,
                           char attribute((unused)) **av) {
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, 0xFFFFFFFF);
  sftp_send_uint32(&fakeworker, 0);          /* id */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_unsupported");
  status();
  return 0;
}

static int cmd_ext_unsupported(int attribute((unused)) ac,
                               char attribute((unused)) **av) {
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_EXTENDED);
  sftp_send_uint32(&fakeworker, 0);          /* id */
  sftp_send_string(&fakeworker, "no-such-sftp-extension@rjk.greenend.org.uk");
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_unsupported");
  status();
  return 0;
}

static int cmd_readlink(int attribute((unused)) ac,
                        char **av) {
  char *r = sftp_readlink(av[0]);

  if(r) {
    xprintf("%s\n", r);
    return 0;
  } else
    return -1;
}

static int cmd_realpath(int attribute((unused)) ac,
                        char **av) {
  char *r = sftp_realpath(av[0]);

  if(r) {
    xprintf("%s\n", r);
    return 0;
  } else
    return -1;
}

static int cmd_realpath6(int attribute((unused)) ac,
                         char **av) {
  int control_byte;
  char *resolved;
  struct sftpattr attrs;
  time_t now;
  struct tm nowtime;

  if(!strcmp(av[0], "no-check"))
    control_byte = SSH_FXP_REALPATH_NO_CHECK;
  else if(!strcmp(av[0], "stat-if"))
    control_byte = SSH_FXP_REALPATH_STAT_IF;
  else if(!strcmp(av[0], "stat-always"))
    control_byte = SSH_FXP_REALPATH_STAT_ALWAYS;
  else
    return error("unknown control string '%s'", av[0]);
  resolved = sftp_realpath_v6(av[1], control_byte, av + 2, &attrs);
  if(!resolved)
    return -1;
  if(attrs.valid) {
    attrs.name = resolved;
    time(&now);
    gmtime_r(&now, &nowtime);
    xprintf("%s\n", sftp_format_attr(fakejob.a, &attrs, nowtime.tm_year, 0));
  } else
    xprintf("%s\n", resolved);
  return 0;
}

static int cmd_lrealpath(int attribute((unused)) ac,
                         char **av) {
  char *r;
  unsigned flags;

  if(!strcmp(av[0], "no-check"))
    flags = 0;
  else if(!strcmp(av[0], "stat-if"))
    flags = RP_READLINK;
  else if(!strcmp(av[0], "stat-always"))
    flags = RP_READLINK|RP_MUST_EXIST;
  else
    return error("unknown control string '%s'", av[0]);
  r = sftp_find_realpath(fakejob.a, av[1], flags);
  if(r) {
    xprintf("%s\n", r);
    return 0;
  } else {
    perror(0);
    return -1;
  }
}

static int cmd_bad_handle(int attribute((unused)) ac,
                          char attribute((unused)) **av) {
  struct handle h;

  h.len = 8;
  h.data = (void *)"\x0\x0\x0\x0\x0\x0\x0\x0";
  sftp_readdir(&h, 0, 0);
  sftp_close(&h);
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_bytes(&fakeworker, h.data, h.len);
  sftp_send_uint64(&fakeworker, 0);
  sftp_send_uint32(&fakeworker, 64);
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_handle");
  status();
  return 0;
}

static int cmd_bad_packet(int attribute((unused)) ac,
                          char attribute((unused)) **av) {
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  /* completely missing ID */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint8(&fakeworker, 0);           /* broken ID */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint8(&fakeworker, 0);           /* truncated handle length */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint32(&fakeworker, 64);         /* truncated handle */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint32(&fakeworker, 0xFFFFFFFF); /* impossibly long handle */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READ);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_string(&fakeworker, "12345678");
  sftp_send_uint32(&fakeworker, 0);          /* truncated uint64 */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READLINK);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint32(&fakeworker, 0xFFFFFFFF); /* impossibly long string */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READLINK);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint32(&fakeworker, 32);         /* truncated string */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_READLINK);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_uint8(&fakeworker, 0);           /* truncated string length */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  return 0;
}

static int cmd_bad_packet456(int attribute((unused)) ac,
                             char attribute((unused)) **av) {
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_SETSTAT);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_string(&fakeworker, "path");
  sftp_send_uint32(&fakeworker, 0);          /* valid attributes */
  /* missing type field */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_packet");
  status();
  return 0;
}

static int cmd_bad_path(int attribute((unused)) ac,
                             char attribute((unused)) **av) {
  sftp_send_begin(&fakeworker);
  sftp_send_uint8(&fakeworker, SSH_FXP_OPENDIR);
  sftp_send_uint32(&fakeworker, 0);
  sftp_send_string(&fakeworker, "\xC0");     /* can't be the start of a UTF-8
                                         * sequence */
  /* missing type field */
  sftp_send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, 0, "_bad_path");
  status();
  return 0;
}

static int cmd_stat(int attribute((unused)) ac,
                    char **av) {
  struct sftpattr attrs;
  time_t now;
  struct tm nowtime;

  if(sftp_stat(av[0], &attrs, SSH_FXP_STAT)) return -1;
  time(&now);
  gmtime_r(&now, &nowtime);
  xprintf("%s\n", sftp_format_attr(fakejob.a, &attrs, nowtime.tm_year, 0));
  return 0;
}

static int cmd_lstat(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  time_t now;
  struct tm nowtime;

  if(sftp_stat(av[0], &attrs, SSH_FXP_LSTAT)) return -1;
  time(&now);
  gmtime_r(&now, &nowtime);
  xprintf("%s\n", sftp_format_attr(fakejob.a, &attrs, nowtime.tm_year, 0));
  return 0;
}

static int cmd_truncate(int attribute((unused)) ac,
                        char **av) {
  struct sftpattr attrs;

  memset(&attrs, 0, sizeof attrs);
  attrs.valid = SSH_FILEXFER_ATTR_SIZE;
  attrs.size = strtoull(av[0], 0, 0);
  return sftp_setstat(av[1], &attrs);
}

static int cmd_overlap(int attribute((unused)) ac,
                       char attribute((unused)) **av) {
  struct sftpattr attrs;
  static const char a[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  static const char b[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  static const char c[] = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
  static const char d[] = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
  static const char expect[] = "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
  char buffer[128];
  int n, r;
  uint32_t ida, idb, idc, idd;
  int fd;
  struct handle h;

  memset(&attrs, 0, sizeof attrs);
  if(sftp_open("dest", ACE4_WRITE_DATA,
               SSH_FXF_CREATE_TRUNCATE, &attrs, &h))
    return -1;
  if((fd = open("dest", O_RDWR)) < 0) {
    perror("open dest");
    return -1;
  }
  for(n = 0; n < 1024; ++n) {
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_WRITE);
    sftp_send_uint32(&fakeworker, ida = newid());
    sftp_send_bytes(&fakeworker, h.data, h.len);
    sftp_send_uint64(&fakeworker, 0);
    sftp_send_bytes(&fakeworker, a, 64);
    sftp_send_end(&fakeworker);
    
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_WRITE);
    sftp_send_uint32(&fakeworker, idb = newid());
    sftp_send_bytes(&fakeworker, h.data, h.len);
    sftp_send_uint64(&fakeworker, 16);
    sftp_send_bytes(&fakeworker, b, 64);
    sftp_send_end(&fakeworker);
    
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_WRITE);
    sftp_send_uint32(&fakeworker, idc = newid());
    sftp_send_bytes(&fakeworker, h.data, h.len);
    sftp_send_uint64(&fakeworker, 32);
    sftp_send_bytes(&fakeworker, c, 64);
    sftp_send_end(&fakeworker);
    
    sftp_send_begin(&fakeworker);
    sftp_send_uint8(&fakeworker, SSH_FXP_WRITE);
    sftp_send_uint32(&fakeworker, idd = newid());
    sftp_send_bytes(&fakeworker, h.data, h.len);
    sftp_send_uint64(&fakeworker, 48);
    sftp_send_bytes(&fakeworker, d, 64);
    sftp_send_end(&fakeworker);

    /* Await responses */
    getresponse(SSH_FXP_STATUS, 0, "SSH_FXP_WRITE");
    getresponse(SSH_FXP_STATUS, 0, "SSH_FXP_WRITE");
    getresponse(SSH_FXP_STATUS, 0, "SSH_FXP_WRITE");
    getresponse(SSH_FXP_STATUS, 0, "SSH_FXP_WRITE");

    if(lseek(fd, 0, SEEK_SET) < 0) {
      perror("lseek");
      return -1;
    }
    r = read(fd, buffer, 128);
    if(r != 48+64) {
      fprintf(stderr, "expected %d bytes got %d\n", 48+64, r);
      return -1;
    }
    if(memcmp(buffer, expect, 48+64)) {
      fprintf(stderr, "buffer contents mismatch\n"
              "expect: %s\n"
              "   got: %.112s\n",
              expect, buffer);
      return -1;
    }
    if(ftruncate(fd, 0) < 0) {
      perror("ftruncate");
      return -1;
    }
  }
  close(fd);
  return 0;
}


/* Table of command line operations */
static const struct command commands[] = {
  {
    "_bad_handle", 0, 0, cmd_bad_handle,
    0,
    "operate on a bogus handle"
  },
  {
    "_bad_packet", 0, 0, cmd_bad_packet,
    0,
    "send bad packets"
  },
  {
    "_bad_packet456", 0, 0, cmd_bad_packet456,
    0,
    "send bad packets (protos >= 4 only)"
  },
  {
    "_bad_path", 0, 0, cmd_bad_path,
    0,
    "send bad paths"
  },
  {
    "_ext_unsupported", 0, 0, cmd_ext_unsupported,
    0,
    "send an unsupported extension"
  },
  {
    "_init", 0, 0, cmd_init,
    0,
    "resend SSH_FXP_INIT"
  },
  {
    "_lrealpath", 2, 2, cmd_lrealpath,
    "CONTROL PATH",
    "expand a local path name"
  },
  {
    "_overlap", 0, 0, cmd_overlap,
    "",
    "test overlapping writes"
  },
  {
    "_unsupported", 0, 0, cmd_unsupported,
    0,
    "send an unsupported command"
  },
  {
    "binary", 0, 0, cmd_binary,
    0,
    "binary mode"
  },
  {
    "bye", 0, 0, cmd_quit,
    0,
    "quit"
  },
  {
    "cd", 1, 1, cmd_cd,
    "DIR",
    "change remote directory"
  },
  {
    "chgrp", 2, 2, cmd_chgrp,
    "GID PATH",
    "change remote file group"
  },
  {
    "chmod", 2, 2, cmd_chmod,
    "OCTAL PATH",
    "change remote file permissions"
  },
  {
    "chown", 2, 2, cmd_chown,
    "UID PATH",
    "change remote file ownership"
  },
  {
    "debug", 0, 0, cmd_debug,
    0,
    "toggle sftp_debugging"
  },
  {
    "df", 0, 1, cmd_df,
    "[PATH]",
    "query available space"
  },
  {
    "exit", 0, 0, cmd_quit,
    0,
    "quit"
  },
  {
    "get", 1, 3, cmd_get,
    "[-PfL<line>] REMOTE-PATH [LOCAL-PATH]",
    "retrieve a remote file"
  },
  {
    "help", 0, 0, cmd_help,
    0,
    "display help"
  },
  {
    "lcd", 1, 1, cmd_lcd,
    "DIR",
    "change local directory"
  },
  {
    "link", 2, 2, cmd_link,
    "OLDPATH NEWPATH",
    "create a remote hard link"
  },
  {
    "lpwd", 0, 0, cmd_lpwd,
    "DIR",
    "display current local directory"
  },
  {
    "lls", 0, INT_MAX, cmd_lls,
    "[OPTIONS] [LOCAL-PATH]",
    "list local directory"
  },
  {
    "lmkdir", 1, 1, cmd_lmkdir,
    "LOCAL-PATH",
    "create local directory"
  },
  {
    "ls", 0, 2, cmd_ls,
    "[OPTIONS] [PATH]",
    "list remote directory"
  },
  {
    "lstat", 1, 1, cmd_lstat,
    "PATH",
    "lstat a file"
  },
  {
    "lumask", 0, 1, cmd_lumask,
    "OCTAL",
    "get or set local umask"
  },
  {
    "mkdir", 1, 2, cmd_mkdir,
    "[MODE] DIRECTORY",
    "create a remote directory"
  },
  {
    "mv", 2, 3, cmd_mv,
    "[-naop] OLDPATH NEWPATH",
    "rename a remote file"
  },
  {
    "progress", 0, 1, cmd_progress,
    "[on|off]",
    "set or toggle progress indicators"
  },
  {
    "put", 1, 3, cmd_put, "[-PaftemMODE] LOCAL-PATH [REMOTE-PATH]",
    "upload a file"
  },
  {
    "pwd", 0, 0, cmd_pwd,
    0,
    "display current remote directory" 
  },
  {
    "quit", 0, 0, cmd_quit,
    0,
    "quit"
  },
  {
    "readlink", 1, 1, cmd_readlink,
    "PATH",
    "inspect a symlink"
  },
  {
    "realpath", 1, 1, cmd_realpath,
    "PATH",
    "expand a path name"
  },
  {
    "realpath6", 2, INT_MAX, cmd_realpath6,
    "CONTROL PATH [COMPOSE...]",
    "expand a path name"
  },
  {
    "rename", 2, 2, cmd_mv,
    "OLDPATH NEWPATH",
    "rename a remote file"
  },
  {
    "rm", 1, 1, cmd_rm,
    "PATH",
    "remove remote file"
  },
  {
    "rmdir", 1, 1, cmd_rmdir,
    "PATH",
    "remove remote directory"
  },
  {
    "symlink", 2, 2, cmd_symlink,
    "TARGET NEWPATH",
    "create a remote symlink"
  },
  {
    "stat", 1, 1, cmd_stat,
    "PATH",
    "stat a file"
  },
  {
    "text", 0, 0, cmd_text,
    0,
    "text mode"
  },
  {
    "truncate", 2, 2, cmd_truncate,
    "LENGTH FILE",
    "truncate a file"
  },
  {
    "version", 0, 1, cmd_version,
    0,
    "set or display protocol version"
  },
  { 0, 0, 0, 0, 0, 0 }
};

static int cmd_help(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  int n;
  size_t max = 0, len = 0;

  for(n = 0; commands[n].name; ++n) {
    if(commands[n].name[0] == '_')
      continue;
    len = strlen(commands[n].name);
    if(commands[n].args)
      len += strlen(commands[n].args) + 1;
    if(len > max) 
      max = len;
  }
  for(n = 0; commands[n].name; ++n) {
    if(commands[n].name[0] == '_')
      continue;
    len = strlen(commands[n].name);
    xprintf("%s", commands[n].name);
    if(commands[n].args) {
      len += strlen(commands[n].args) + 1;
      xprintf(" %s", commands[n].args);
    }
    xprintf("%*s  %s\n", (int)(max - len), "", commands[n].help);
  }
  return 0;
}

static char *input(const char *prompt, FILE *fp) {
  char buffer[4096];

  if(prompt) {
#if HAVE_READLINE
    char *const s = readline(prompt);

    if(s) {
      const char *t = s;

      while(isspace((unsigned char)*t))
        ++t;
      if(*t)
        add_history(s);
    }
    return s;
#else
    xprintf("%s", prompt);
    if(fflush(stdout) < 0)
      fatal("error calling fflush: %s", strerror(errno));
#endif
  }
  if(!fgets(buffer, sizeof buffer, fp))
    return 0;
  return xstrdup(buffer);
}

/* Input processing loop */
static void process(const char *prompt, FILE *fp) {
  char *line;
  int ac, n;
  char *avbuf[256], **av;
 
  while((line = input(prompt, fp))) {
    ++inputline;
    if(line[0] == '#')
      goto next;
    if(echo) {
      xprintf("%s", line);
      if(fflush(stdout) < 0)
        fatal("error calling fflush: %s", strerror(errno));
    }
    if(line[0] == '!') {
      if(line[1] != '\n')
        system(line + 1);
      else
        system(getenv("SHELL"));
      goto next;
    }
    if((ac = split(line, av = avbuf)) < 0) {
      if(stoponerror)
        fatal("stopping on error");
      goto next;
    }
    if(!ac)
      goto next;
    for(n = 0; commands[n].name && strcmp(av[0], commands[n].name); ++n)
      ;
    if(!commands[n].name) {
      error("unknown command: '%s'", av[0]);
      if(stoponerror)
        fatal("stopping on error");
      goto next;
    }
    ++av;
    --ac;
    if(ac < commands[n].minargs || ac > commands[n].maxargs) {
      error("wrong number of arguments (got %d, want %d-%d)",
            ac, commands[n].minargs, commands[n].maxargs);
      if(stoponerror)
        fatal("stopping on error");
      goto next;
    }
    if(commands[n].handler(ac, av) && stoponerror)
      fatal("stopping on error");
next:
    if(fflush(stdout) < 0)
      fatal("error calling fflush: %s", strerror(errno));
    sftp_alloc_destroy(fakejob.a);
    free(line);
  }
  if(ferror(fp))
    fatal("error reading %s: %s", inputpath, strerror(errno));
  if(prompt)
    xprintf("\n");
}

int main(int argc, char **argv) {
  int n;
#if HAVE_GETADDRINFO
  struct addrinfo hints;
#endif
  const char *host = 0, *port = 0;

#if HAVE_GETADDRINFO
  memset(&hints, 0, sizeof hints);
  hints.ai_flags = 0;
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
#endif

  setlocale(LC_ALL, "");

  /* Figure out terminal width */
  {
    struct winsize ws;
    const char *e;

    if((e = getenv("COLUMNS")))
      terminal_width = (size_t)strtoul(e, 0, 10);
    else if(ioctl(1, TIOCGWINSZ, &ws) >= 0)
      terminal_width = ws.ws_col;
    else
      terminal_width = 80;
  }

  while((n = getopt_long(argc, argv, "hVB:b:P:R:s:S:12CF:o:vdH:p:46D:",
			 options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'B': buffersize = atoi(optarg); break;
    case 'b': batchfile = optarg; stoponerror = 1; progress_indicators = 0; break;
    case 'P': program = optarg; break;
    case 'R': nrequests = atoi(optarg); break;
    case 's': subsystem = optarg; break;
    case 'S': sftpversion = atoi(optarg); break;
    case '1': sshversion = 1; break;
    case '2': sshversion = 2; break;
    case 'C': compress = 1; break;
    case 'F': sshconf = optarg; break;
    case 'o': sshoptions[nsshoptions++] = optarg; break;
    case 'v': sshverbose++; break;
    case 'd': sftp_debugging = 1; break;
    case 'D': sftp_debugging = 1; sftp_debugpath = optarg; break;
    case 256: quirk_reverse_symlink = 1; break;
    case 257: stoponerror = 1; break;
    case 258: stoponerror = 0; break;
    case 259: progress_indicators = 1; break;
    case 260: progress_indicators = 0; break;
    case 261: echo = 1; break;
    case 262: signal(SIGPIPE, SIG_DFL); break; /* stupid python */
    case 263: sftpversion = atoi(optarg); forceversion = 1; break;
    case 'H': host = optarg; break;
    case 'p': port = optarg; break;
#if HAVE_GETADDRINFO
    case '4': hints.ai_family = PF_INET; break;
    case '6': hints.ai_family = PF_INET6; break;
#endif
    default: exit(1);
    }
  }

  /* sanity checking */
  if(nrequests <= 0)
    nrequests = 1;
  if(nrequests > 128)
    nrequests = 128;
  if(buffersize < 64)
    buffersize = 64;
  if(buffersize > 1048576)
    buffersize = 1048576;

  if((sftpversion < 3 || sftpversion > 6) && !forceversion)
    fatal("unknown SFTP version %d", sftpversion);
  
  if(host || port) {
#if HAVE_GETADDRINFO
    struct addrinfo *res;
    int rc, fd;

    if(!(host && port) || program || subsystem)
      fatal("inconsistent options");
    if((rc = getaddrinfo(host, port, &hints, &res)))
      fatal("error resolving host %s port %s: %s",
            host, port, gai_strerror(rc));
    if((fd = socket(res->ai_family, res->ai_socktype,
                    res->ai_protocol)) < 0)
      fatal("error calling socket: %s", strerror(errno));
    if(connect(fd, res->ai_addr, res->ai_addrlen) < 0)
      fatal("error connecting to host %s port %s: %s",
            host, port, strerror(errno));
    sftpin = sftpout = fd;
#else
    /* It's hardly a core feature... */
    fatal("--host is not supported on this platform");
#endif
  } else {
    const char *cmdline[2048];
    int ncmdline = 0;
    int ip[2], op[2];
    pid_t pid;

    if(program) {
      cmdline[ncmdline++] = program;
    } else {
      cmdline[ncmdline++] = "ssh";
      if(optind >= argc)
        fatal("missing USER@HOST argument");
      if(sshversion == 1)
        cmdline[ncmdline++] = "-1";
      if(sshversion == 2)
        cmdline[ncmdline++] = "-2";
      if(compress)
        cmdline[ncmdline++] = "-C";
      if(sshconf) {
        cmdline[ncmdline++] = "-F";
        cmdline[ncmdline++] = sshconf;
      }
      for(n = 0; n < nsshoptions; ++n) {
        cmdline[ncmdline++] = "-o";
        cmdline[ncmdline++] = sshoptions[n++];
      }
      while(sshverbose-- > 0)
        cmdline[ncmdline++] = "-v";
      cmdline[ncmdline++] = "-s";
      cmdline[ncmdline++] = argv[optind++];
      cmdline[ncmdline++] = subsystem ? subsystem : "sftp";
    }
    cmdline[ncmdline] = 0;
    xpipe(ip);
    xpipe(op);
    if(!(pid = xfork())) {
      xclose(ip[0]);
      xclose(op[1]);
      xdup2(ip[1], 1);
      xdup2(op[0], 0);
      execvp(cmdline[0], (void *)cmdline);
      fatal("executing %s: %s", cmdline[0], strerror(errno));
    }
    xclose(ip[1]);
    xclose(op[0]);
    sftpin = ip[0];
    sftpout = op[1];
  }
  fakejob.a = sftp_alloc_init(&allocator);
  fakejob.worker = &fakeworker;
  if((fakeworker.utf8_to_local = iconv_open(nl_langinfo(CODESET), "UTF-8"))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));
  if((fakeworker.local_to_utf8 = iconv_open("UTF-8", nl_langinfo(CODESET)))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));

  if(sftp_init())
    return 1;
  

  if(batchfile) {
    FILE *fp;

    inputpath = batchfile;
    if(!(fp = fopen(batchfile, "r")))
      fatal("error opening %s: %s", batchfile, strerror(errno));
    process(0, fp);
  } else {
    inputpath = "stdin";
    process("sftp> ", stdin);
  }
  /* We let the OS reap the SSH process */
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
