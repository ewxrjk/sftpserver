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
static const struct command commands[];

const struct sftpprotocol *protocol = &sftpv3;
const char sendtype[] = "request";

/* Command line */
static size_t buffersize = 8192;
static int nrequests = 8;
static const char *subsystem = "sftp";
static const char *program;
static const char *batchfile;
static int sshversion;
static int compress;
static const char *sshconf;
static const char *sshoptions[1024];
static int nsshoptions;
static int sshverbose;
static int sftpversion = 3;
static int quirk_openssh;

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "buffer", required_argument, 0, 'B' },
  { "batch", required_argument, 0, 'b' },
  { "program", required_argument, 0, 'P' },
  { "requests", required_argument, 0, 'R' },
  { "subsystem", required_argument, 0, 's' },
  { "sftp-version", required_argument, 0, 'S' },
  { "quirk-openssh", no_argument, 0, 256 },
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
	 "\n"
	 "Options:\n"
	 "  --help, -h               Display usage message\n"
	 "  --version, -V            Display version number\n"
	 "  -B, --buffer BYTES       Select buffer size (default 32768)\n"
	 "  -b, --batch PATH         Read batch file\n"
	 "  -P, --program PATH       Execute program as SFTP server\n"
	 "  -R, --requests COUNT     Maximum outstanding requests (default 8)\n"
	 "  -s, --subsystem NAME     Remote subsystem name\n"
	 "  -S, --sftp-version VER   Protocol version to request (default 3)\n"
         "  --quirk-openssh          Server gets SSH_FXP_SYMLINK backwards\n"
	 "Options passed to SSH:\n"
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

static void attribute((format(printf,1,2))) error(const char *fmt, ...) {
  va_list ap;

  fprintf(stderr, "%s:%d ", inputpath, inputline);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n',  stderr);
}

static int status(void) {
  uint32_t status;
  char *msg;
  
  /* Cope with half-parsed responses */
  fakejob.ptr = fakejob.data + 5;
  fakejob.left = fakejob.len - 5;
  cpcheck(parse_uint32(&fakejob, &status));
  cpcheck(parse_string(&fakejob, &msg, 0));
  if(status) {
    error("%s (%s)", msg, status_to_string(status));
    return -1;
  } else
    return 0;
}

/* Get a response.  Picks out the type and ID. */
static uint8_t getresponse(int expected, uint32_t expected_id) {
  uint32_t len;
  uint8_t type;

  if(do_read(sftpin, &len, sizeof len))
    fatal("unexpected EOF from server while reading length");
  free(fakejob.data);                   /* free last job */
  fakejob.len = ntohl(len);
  fakejob.data = xmalloc(fakejob.len);
  if(do_read(sftpin, fakejob.data, fakejob.len))
    fatal("unexpected EOF from server while reading data");
  if(DEBUG) {
    D(("response:"));
    hexdump(fakejob.data, fakejob.len > 32 ? 32 : fakejob.len);
  }
  fakejob.left = fakejob.len;
  fakejob.ptr = fakejob.data;
  cpcheck(parse_uint8(&fakejob, &type));
  if(type != SSH_FXP_VERSION) {
    cpcheck(parse_uint32(&fakejob, &fakejob.id));
    if(expected_id && fakejob.id != expected_id)
      fatal("wrong ID in response (want %"PRIu32" got %"PRIu32,
            expected_id, fakejob.id);
  }
  if(expected > 0 && type != expected) {
    if(type == SSH_FXP_STATUS)
      status();
    else
      fatal("expected response %d got %d", expected, type);
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
      arg = av[ac++] = line;
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

static const char *resolvepath(const char *name) {
  char *resolved;

  if(name[0] == '/') return name;
  resolved = alloc(fakejob.a, strlen(cwd) + strlen(name) + 2);
  sprintf(resolved, "%s/%s", cwd, name);
  return resolved;
}

static void progress(const char *path, uint64_t sofar, uint64_t total) {
  if(!total)
    printf("\r                                                                                \r");
  else if(total == (uint64_t)-1)
    printf("\r%.60s: %12"PRIu64"b", path, sofar);
  else
    printf("\r%.60s: %12"PRIu64"b %3d%%", path, sofar, (int)(100 * sofar / total));
  fflush(stdout);
}

/* SFTP operation stubs */

static char *sftp_realpath(const char *path) {
  char *resolved;
  uint32_t u32, id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_REALPATH);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, path);
  send_end(&fakejob);
  if(getresponse(SSH_FXP_NAME, id) != SSH_FXP_NAME)
    return 0;
  cpcheck(parse_uint32(&fakejob, &u32));
  if(u32 != 1) fatal("wrong count in SSH_FXP_REALPATH reply");
  cpcheck(parse_path(&fakejob, &resolved));
  return resolved;
}

static int sftp_stat(const char *path, struct sftpattr *attrs,
                     uint8_t type) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, type);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  send_end(&fakejob);
  if(getresponse(SSH_FXP_ATTRS, id) != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  attrs->name = path;
  return 0;
}

static int sftp_fstat(const struct handle *hp, struct sftpattr *attrs) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_FSTAT);
  send_uint32(&fakejob, id = newid());
  send_bytes(&fakejob, hp->data, hp->len);
  send_end(&fakejob);
  if(getresponse(SSH_FXP_ATTRS, id) != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  return 0;
}

static int sftp_opendir(const char *path, struct handle *hp) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_OPENDIR);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  send_end(&fakejob);
  if(getresponse(SSH_FXP_HANDLE, id) != SSH_FXP_HANDLE)
    return -1;
  cpcheck(parse_string(&fakejob, &hp->data, &hp->len));
  return 0;
}

static int sftp_readdir(const struct handle *hp,
                        struct sftpattr **attrsp,
                        size_t *nattrsp) {
  uint32_t id, n;
  struct sftpattr *attrs;
  char *name, *longname;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_READDIR);
  send_uint32(&fakejob, id = newid());
  send_bytes(&fakejob, hp->data, hp->len);
  send_end(&fakejob);
  switch(getresponse(-1, id)) {
  case SSH_FXP_NAME:
    cpcheck(parse_uint32(&fakejob, &n));
    if(n > SIZE_MAX / sizeof(struct sftpattr))
      fatal("too many attributes in SSH_FXP_READDIR response");
    attrs = alloc(fakejob.a, n * sizeof(struct sftpattr));
    *nattrsp = n;
    *attrsp = attrs;
    while(n-- > 0) {
      cpcheck(parse_path(&fakejob, &name));
      cpcheck(parse_path(&fakejob, &longname));
      cpcheck(protocol->parseattrs(&fakejob, attrs));
      attrs->name = name;
      attrs->longname = longname;
      ++attrs;
    }
    return 0;
  case SSH_FXP_STATUS:
    cpcheck(parse_uint32(&fakejob, &n));
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

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_CLOSE);
  send_uint32(&fakejob, id = newid());
  send_bytes(&fakejob, hp->data, hp->len);
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_setstat(const char *path,
                        const struct sftpattr *attrs) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_SETSTAT);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  protocol->sendattrs(&fakejob, attrs);
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_rmdir(const char *path) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_RMDIR);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_remove(const char *path) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_REMOVE);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_rename(const char *oldpath, const char *newpath) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_RENAME);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(oldpath));
  send_path(&fakejob, resolvepath(newpath));
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_symlink(const char *targetpath, const char *linkpath) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_SYMLINK);
  send_uint32(&fakejob, id = newid());
  if(quirk_openssh && sftpversion == 3) {
    /* OpenSSH server gets SSH_FXP_SYMLINK args back to front
     * - see http://bugzilla.mindrot.org/show_bug.cgi?id=861 */
    send_path(&fakejob, resolvepath(targetpath));
    send_path(&fakejob, resolvepath(linkpath));
  } else {
    send_path(&fakejob, resolvepath(linkpath));
    send_path(&fakejob, resolvepath(targetpath));
  }
  send_end(&fakejob);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_open(const char *path, uint32_t flags, 
                     const struct sftpattr *attrs,
                     struct handle *hp) {
  uint32_t id;

  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_OPEN);
  send_uint32(&fakejob, id = newid());
  send_path(&fakejob, resolvepath(path));
  /* For the time being we just send the v3 values.  We'll have to do somethnig
   * cleverer when we support later protocol version. */
  send_uint32(&fakejob, flags);
  protocol->sendattrs(&fakejob, attrs);
  send_end(&fakejob);
  if(getresponse(SSH_FXP_HANDLE, id) != SSH_FXP_HANDLE)
    return -1;
  cpcheck(parse_string(&fakejob, &hp->data, &hp->len));
  return 0;
}

/* Command line operations */

static int cmd_pwd(int attribute((unused)) ac,
                   char attribute((unused)) **av) {
  xprintf("%s\n", cwd);
  return 0;
}

static int cmd_cd(int attribute((unused)) ac,
                  char **av) {
  const char *newcwd;
  struct sftpattr attrs;

  newcwd = sftp_realpath(resolvepath(av[0]));
  if(!newcwd) return -1;
  /* Check it's really a directory */
  if(sftp_stat(newcwd, &attrs, SSH_FXP_LSTAT)) return -1;
  if(attrs.type != SSH_FILEXFER_TYPE_DIRECTORY) {
    error("%s is not a directory", av[0]);
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
                    char attribute((unused)) **av) {
  int n;
  size_t max = 0, len = 0;

  for(n = 0; commands[n].name; ++n) {
    len = strlen(commands[n].name);
    if(commands[n].args)
      len += strlen(commands[n].args) + 1;
    if(len > max) 
      max = len;
  }
  for(n = 0; commands[n].name; ++n) {
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

static int cmd_lpwd(int attribute((unused)) ac,
                    char attribute((unused)) **av) {
  char *buffer = alloc(fakejob.a, PATH_MAX + 1);
  if(!(getcwd(buffer, PATH_MAX + 1))) {
    error("error calling getcwd: %s", strerror(errno));
    return -1;
  }
  xprintf("%s\n", buffer);
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
    if(a->size < b->size) return -1;
    else if(a->size > b->size) return 1;
  }
  return sort_by_name(av, bv);
}

static int sort_by_mtime(const void *av, const void *bv) {
  const struct sftpattr *const a = av, *const b = bv;

  if(a->valid & b->valid & SSH_FILEXFER_ATTR_MODIFYTIME) {
    if(a->mtime.seconds < b->mtime.seconds) return -1;
    else if(a->mtime.seconds > b->mtime.seconds) return 1;
    if(a->valid & b->valid & SSH_FILEXFER_ATTR_SUBSECOND_TIMES) {
      if(a->mtime.nanoseconds < b->mtime.nanoseconds) return -1;
      else if(a->mtime.nanoseconds > b->mtime.nanoseconds) return 1;
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
  size_t nattrs, nallattrs = 0, n, maxnamelen = 0;
  struct handle h;

  if(ac > 0 && av[0][0] == '-') {
    options = *av++;
    --ac;
  } else
    options = "";
  path = ac > 0 ? av[0] : cwd;
  /* See what type the file is */
  if(sftp_stat(path, &fileattrs, SSH_FXP_STAT)) return -1;
  if(fileattrs.type != SSH_FILEXFER_TYPE_DIRECTORY
     || strchr(options, 'd')) {
    /* The file is not a directory, or we used -d */
    allattrs = &fileattrs;
    nallattrs = 1;
  } else {
    /* The file is a directory and we did not use -d */
    if(sftp_opendir(path, &h)) return -1;
    for(;;) {
      if(sftp_readdir(&h, &attrs, &nattrs)) {
        sftp_close(&h);
        free(allattrs);
        return -1;
      }
      if(!nattrs) break;                  /* eof */
      allattrs = xrecalloc(allattrs, nattrs + nallattrs, sizeof *attrs);
      if(strchr(options, 'a')) {
        /* Include dotfiles */
        for(n = 0; n < nattrs; ++n) {
          if(strlen(attrs[n].name) > maxnamelen)
            maxnamelen = strlen(attrs[n].name);
          allattrs[nallattrs++] = attrs[n];
        }
      } else {
        /* Exclude dotfiles */
        for(n = 0; n < nattrs; ++n) {
          if(attrs[n].name[0] == '.')
            continue;
          if(strlen(attrs[n].name) > maxnamelen)
            maxnamelen = strlen(attrs[n].name);
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
                                 : 0);
    
    /* We'd like to know what year we're in for dates in longname */
    time(&now);
    gmtime_r(&now, &nowtime);
    for(n = 0; n < nallattrs; ++n)
      xprintf("%s\n", format_attr(fakejob.a, &allattrs[n], nowtime.tm_year,
                                  flags));
  } else if(strchr(options, '1')) {
    /* single-column listing */
    for(n = 0; n < nallattrs; ++n)
      xprintf("%s\n", allattrs[n].name);
  } else {
    /* multi-column listing */
    /* This isn't really a production client, we just issue a single-column
     * listing for now */
    for(n = 0; n < nallattrs; ++n)
      xprintf("%s\n", allattrs[n].name);
  }
  if(allattrs != &fileattrs)
    free(allattrs);
  return 0;
}

static int cmd_lls(int ac,
                   char **av) {
  const char **args = alloc(fakejob.a, (ac + 2) * sizeof (char *));
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
  
  if(sftp_stat(av[1], &attrs, SSH_FXP_STAT)) return -1;
  attrs.valid = SSH_FILEXFER_ATTR_UIDGID;
  attrs.uid = atoi(av[0]);
  return sftp_setstat(av[1], &attrs);
}

static int cmd_chgrp(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  
  if(sftp_stat(av[1], &attrs, SSH_FXP_STAT)) return -1;
  attrs.valid = SSH_FILEXFER_ATTR_UIDGID;
  attrs.gid = atoi(av[0]);
  return sftp_setstat(av[1], &attrs);
}

static int cmd_chmod(int attribute((unused)) ac,
                     char **av) {
  struct sftpattr attrs;
  
  attrs.valid = SSH_FILEXFER_ATTR_PERMISSIONS;
  errno = 0;
  attrs.permissions = strtol(av[0], 0, 8);
  if(errno || attrs.permissions != (attrs.permissions & 0777))
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
  return sftp_rename(av[0], av[1]);
}

static int cmd_symlink(int attribute((unused)) ac,
                       char **av) {
  return sftp_symlink(av[0], av[1]);
}

/* cmd_get uses a background thread to send requests */
struct outstanding_request {
  uint32_t id;                          /* 0 or a request ID */
  off_t offset;                         /* offset in source file */
};

struct reader_data {
  pthread_mutex_t m;                    /* protects everything here */
  pthread_cond_t c1;                    /* signaled when a response received */
  pthread_cond_t c2;                    /* signaled when a request sent */
  struct handle h;                      /* target handle */
  struct outstanding_request *reqs;     /* in-flight requests */
  uint64_t next_offset;                 /* next offset */
  int outstanding, eof, failed;
  uint64_t size;                        /* file size */
};

static void *reader_thread(void *arg) {
  struct reader_data *const r = arg;
  int n;
  uint32_t id, len;
  
  ferrcheck(pthread_mutex_lock(&r->m));
  while(!r->eof && !r->failed) {
    /* Send as many jobs as we can */
    while(r->outstanding < nrequests && !r->eof) {
      /* Find a spare slot */
      for(n = 0; n < nrequests && r->reqs[n].id; ++n)
        ;
      assert(n < nrequests);
      id = newid();
      send_begin(&fakejob);
      send_uint8(&fakejob, SSH_FXP_READ);
      send_uint32(&fakejob, id);
      send_bytes(&fakejob, r->h.data, r->h.len);
      send_uint64(&fakejob, r->next_offset);
      if(r->size - r->next_offset > buffersize)
        len = buffersize;
      else {
        len = (uint32_t)(r->size - r->next_offset);
        r->eof = 1;
      }
      send_uint32(&fakejob, len);
      /* Don't hold the lock while doing the send itself */
      ferrcheck(pthread_mutex_unlock(&r->m));
      send_end(&fakejob);
      ferrcheck(pthread_mutex_lock(&r->m));
      /* Only fill in the outstanding_request once we've sent it */
      r->reqs[n].id = id;
      r->reqs[n].offset = r->next_offset;
      ++r->outstanding;
      r->next_offset += buffersize;
      /* Notify the main threader that we set off a request */
      ferrcheck(pthread_cond_signal(&r->c2));
    }
    /* Wait for a job to be reaped */
    ferrcheck(pthread_cond_wait(&r->c1, &r->m));
  }
  ferrcheck(pthread_mutex_unlock(&r->m));
  return 0;
}

static int cmd_get(int ac,
                   char **av) {
  int preserve = 0;
  const char *remote, *local, *e;
  char *tmp = 0;
  struct reader_data r;
  struct sftpattr attrs;
  int fd = -1, n, rc;
  uint8_t rtype;
  uint32_t st, len;
  pthread_t tid;
  uint64_t written = 0;

  memset(&attrs, 0, sizeof attrs);
  memset(&r, 0, sizeof r);
  if(!strcmp(*av, "-P")) {
    preserve = 1;
    ++av;
    --ac;
  }
  remote = *av++;
  --ac;
  if(ac) {
    local = *av++;
    --ac;
  } else
    local = basename(remote);
  /* we'll write to a temporary file */
  tmp = alloc(fakejob.a, strlen(local) + 5);
  sprintf(tmp, "%s.new", local);
  if((fd = open(tmp, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0)
    goto error;
  /* open the remote file */
  if(sftp_open(remote, SSH_FXF_READ, &attrs, &r.h))
    goto error;
  /* stat the file */
  if(sftp_fstat(&r.h, &attrs))
    goto error;
  if(attrs.valid & SSH_FILEXFER_ATTR_SIZE) {
    /* We know how big the file is.  Check we can fit it! */
    if((uint64_t)(off_t)attrs.size != attrs.size) {
      fprintf(stderr, "remote file %s is too large (%"PRIu64" bytes)\n",
              remote, attrs.size);
      goto error;
    }
    r.size = attrs.size;
  } else
    /* We don't know how big the file is.  We'll just keep on reading until we
     * get an EOF. */
    r.size = (uint64_t)-1;
  ferrcheck(pthread_mutex_init(&r.m, 0));
  ferrcheck(pthread_cond_init(&r.c1, 0));
  ferrcheck(pthread_cond_init(&r.c2, 0));
  r.reqs = alloc(fakejob.a, nrequests * sizeof *r.reqs);
  ferrcheck(pthread_create(&tid, 0, reader_thread, &r));
  ferrcheck(pthread_mutex_lock(&r.m));
  /* If there are requests in flight, we must keep going whatever else
   * is happening in order to process their replies.
   * If there are no requests in flight then we keep going until
   * we reach EOF or an error is detected.
   */
  while(r.outstanding || (!r.eof && !r.failed)) {
    /* Wait until there's at least one request in flight */
    while(!r.outstanding)
      ferrcheck(pthread_cond_wait(&r.c2, &r.m));
    /* Don't hold the lock while waiting for a response */
    ferrcheck(pthread_mutex_unlock(&r.m));
    rtype = getresponse(-1, 0);
    ferrcheck(pthread_mutex_lock(&r.m));
    /* Count down the number of requests in flight */
    --r.outstanding;
    /* If we have failed we just have to discard remaining responses */
    if(r.failed) 
      continue;
    switch(rtype) {
    case SSH_FXP_STATUS:
      cpcheck(parse_uint32(&fakejob, &st));
      if(st == SSH_FX_EOF)
        r.eof = 1;
      else {
        status();
        r.failed = 1;
      }
      break;
    case SSH_FXP_DATA:
      /* Find the right request */
      for(n = 0; n < nrequests && fakejob.id != r.reqs[n].id; ++n)
        ;
      assert(n < nrequests);
      /* We don't fully parse the string but instead write it out from the
       * input buffer it's sitting in. */
      cpcheck(parse_uint32(&fakejob, &len));
      /* We don't care what order the responses come in, we just write them to
       * the right place in the file according to what we asked for.  There's
       * not much point releasing the lock while doing the write - the
       * background thread will have done all it wants while we were awaiting
       * the response. */
      rc = pwrite(fd, fakejob.ptr, len, r.reqs[n].offset);
      if(rc < 0) {
        fprintf(stderr, "error writing to %s: %s\n", tmp, strerror(errno));
        r.failed = 1;
      }
      written += len;
      progress(local, written, r.size);
      /* Free up this slot */
      r.reqs[n].id = 0;
      break;
    default:
      fatal("unexpected response %d to SSH_FXP_READ", rtype);
    }
    /* We either set one of r.eof or r.failed, or reaped a response.  So notify
     * the background thread. */ 
    ferrcheck(pthread_cond_signal(&r.c1));
  }
  ferrcheck(pthread_mutex_unlock(&r.m));
  /* Wait for the background thread to finish up */
  ferrcheck(pthread_join(tid, 0));
  /* Tear all the thread objects down */
  ferrcheck(pthread_mutex_destroy(&r.m));
  ferrcheck(pthread_cond_destroy(&r.c1));
  ferrcheck(pthread_cond_destroy(&r.c2));
  progress(0, 0, 0);
  if(r.failed) 
    goto error;
  /* Close the handle */
  sftp_close(&r.h);
  r.h.len = 0;
  if(preserve) {
    /* Set permissions etc */
    attrs.valid &= ~SSH_FILEXFER_ATTR_SIZE; /* don't truncate */
    attrs.valid &= ~SSH_FILEXFER_ATTR_UIDGID; /* different mapping! */
    if((e = set_fstatus(fd, &attrs))) {
      error("cannot %s %s: %s", e, tmp, strerror(errno));
      goto error;
    }
  }
  /* TODO download progress */
  if(close(fd) < 0) {
    error("error closing %s: %s", tmp, strerror(errno));
    fd = -1;
    goto error;
  }
  if(rename(tmp, local) < 0) {
    error("error renaming %s: %s", tmp, strerror(errno));
    goto error;
  }
  return 0;
error:
  if(fd >= 0) close(fd);
  if(tmp) unlink(tmp);
  if(r.h.len) sftp_close(&r.h);
  return -1;
}

/* Table of command line operations */
static const struct command commands[] = {
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
    "exit", 0, 0, cmd_quit,
    0,
    "quit"
  },
  {
    "get", 1, 3, cmd_get,
    "[-P] PATH [LOCAL-PATH]",
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
    "lpwd", 0, 0, cmd_lpwd,
    "DIR",
    "display current local directory"
  },
  {
    "lls", 0, INT_MAX, cmd_lls,
    "[OPTIONS] [PATH]",
    "list local directory"
  },
  {
    "lmkdir", 1, 1, cmd_lmkdir,
    "PATH",
    "create local directory"
  },
  {
    "ls", 0, 2, cmd_ls,
    "[OPTIONS] [PATH]",
    "list remote directory"
  },
  {
    "lumask", 0, 1, cmd_lumask,
    "OCTAL",
    "get or set local umask"
  },
  {
    "mv", 2, 2, cmd_mv,
    "OLDPATH NEWPATH",
    "rename a remote file"
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
  { 0, 0, 0, 0, 0, 0 }
};

/* Input processing loop */
static void process(const char *prompt, FILE *fp) {
  char buffer[4096];
  int ac, n;
  char *avbuf[256], **av;
 
  if(prompt) {
    fputs(prompt, stdout);
    fflush(stdout);
  }
  while(fgets(buffer, sizeof buffer, fp)) {
    ++inputline;
    if(buffer[0] == '!') {
      if(buffer[1] != '\n')
        system(buffer + 1);
      else
        system(getenv("SHELL"));
      goto next;
    }
    if((ac = split(buffer, av = avbuf)) < 0 && !prompt)
      exit(1);
    if(!ac) goto next;
    for(n = 0; commands[n].name && strcmp(av[0], commands[n].name); ++n)
      ;
    if(!commands[n].name) {
      error("unknown command: '%s'", av[0]);
      if(!prompt) exit(1);
      goto next;
    }
    ++av;
    --ac;
    if(ac < commands[n].minargs || ac > commands[n].maxargs) {
      error("wrong number of arguments");
      if(!prompt) exit(1);
      goto next;
    }
    if(commands[n].handler(ac, av) && !prompt)
      exit(1);
next:
    alloc_destroy(fakejob.a);
    if(prompt) {
      fputs(prompt, stdout);
      fflush(stdout);
    }
  }
  if(ferror(fp))
    fatal("error reading %s: %s", inputpath, strerror(errno));
}

int main(int argc, char **argv) {
  const char *cmdline[2048];
  int n, ncmdline;
  int ip[2], op[2];
  pid_t pid;
  uint32_t u32;
  
  while((n = getopt_long(argc, argv, "hVB:b:P:R:s:S:12CF:o:v",
			 options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'B': buffersize = atoi(optarg); break;
    case 'b': batchfile = optarg; break;
    case 'P': program = optarg; break;
    case 'R': nrequests = atoi(optarg); break;
    case 's': subsystem = optarg;
    case 'S': sftpversion = atoi(optarg); break;
    case '1': sshversion = 1; break;
    case '2': sshversion = 2; break;
    case 'C': compress = 1; break;
    case 'F': sshconf = optarg; break;
    case 'o': sshoptions[nsshoptions++] = optarg; break;
    case 'v': sshverbose++; break;
    case 256: quirk_openssh = 1; break;
    default: exit(1);
    }
  }

  /* sanity checking */
  if(nrequests <= 0) nrequests = 1;
  if(nrequests > 128) nrequests = 128;
  if(buffersize < 64) buffersize = 64;
  if(buffersize > 1048576) buffersize = 1048576;

  if(sftpversion != 3)
    fatal("unknown SFTP version %d", sftpversion);
  
  ncmdline = 0;
  if(program) {
    cmdline[ncmdline++] = program;
  } else {
    cmdline[ncmdline++] = "ssh";
    if(optind >= argc)
      fatal("missing USER@HOST argument");
    if(sshversion == 1) cmdline[ncmdline++] = "-1";
    if(sshversion == 2) cmdline[ncmdline++] = "-2";
    if(compress) cmdline[ncmdline++] = "-C";
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
    cmdline[ncmdline++] = subsystem;
  }
  cmdline[ncmdline] = 0;
  for(n = 0; n < ncmdline; ++n)
    fprintf(stderr, " %s", cmdline[n]);
  fputc('\n',  stderr);
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
  fakejob.a = alloc_init(&allocator);
  fakejob.worker = &fakeworker;

  /* Send SSH_FXP_INIT */
  send_begin(&fakejob);
  send_uint8(&fakejob, SSH_FXP_INIT);
  send_uint32(&fakejob, sftpversion);
  send_end(&fakejob);
  
  /* Parse the version reponse */
  getresponse(SSH_FXP_VERSION, 0);
  cpcheck(parse_uint32(&fakejob, &u32));
  if(u32 != 3)
    fatal("we only know protocol version 3 but server wanted version %"PRIu32,
          u32);
  /* TODO parse extensions */

  /* Find path to current directory */
  if(!(cwd = sftp_realpath("."))) exit(1);
  cwd = xstrdup(cwd);

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
