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
static int progress_indicators = 1;
static int terminal_width;
static int textmode;
static const char *newline = "\r\n";

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
static int sftpversion = 4;
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
  { "debug", no_argument, 0, 'd' },
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
	 "  -B, --buffer BYTES       Select buffer size (default 8192)\n"
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

static int attribute((format(printf,1,2))) error(const char *fmt, ...) {
  va_list ap;

  fprintf(stderr, "%s:%d ", inputpath, inputline);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n',  stderr);
  return -1;
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
  if(debugging) {
    D(("response:"));
    hexdump(fakejob.data, fakejob.len);
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
  if(progress_indicators) {
    if(!total)
      printf("\r%*s\r", terminal_width, "");
    else if(total == (uint64_t)-1)
      printf("\r%.60s: %12"PRIu64"b", path, sofar);
    else
      printf("\r%.60s: %12"PRIu64"b %3d%%",
             path, sofar, (int)(100 * sofar / total));
    fflush(stdout);
  }
}

/* SFTP operation stubs */

static char *sftp_realpath(const char *path) {
  char *resolved;
  uint32_t u32, id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_REALPATH);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, path);
  send_end(&fakeworker);
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

  send_begin(&fakeworker);
  send_uint8(&fakeworker, type);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  if(protocol->version > 3)
    send_uint32(&fakeworker, 0xFFFFFFFF);
  send_end(&fakeworker);
  if(getresponse(SSH_FXP_ATTRS, id) != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  attrs->name = path;
  attrs->wname = convertm2w(attrs->name);
  return 0;
}

static int sftp_fstat(const struct handle *hp, struct sftpattr *attrs) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_FSTAT);
  send_uint32(&fakeworker, id = newid());
  send_bytes(&fakeworker, hp->data, hp->len);
  if(protocol->version > 3)
    send_uint32(&fakeworker, 0xFFFFFFFF);
  send_end(&fakeworker);
  if(getresponse(SSH_FXP_ATTRS, id) != SSH_FXP_ATTRS)
    return -1;
  cpcheck(protocol->parseattrs(&fakejob, attrs));
  return 0;
}

static int sftp_opendir(const char *path, struct handle *hp) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_OPENDIR);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  send_end(&fakeworker);
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
  char *name, *longname = 0;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_READDIR);
  send_uint32(&fakeworker, id = newid());
  send_bytes(&fakeworker, hp->data, hp->len);
  send_end(&fakeworker);
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
      if(protocol->version <= 3)
        cpcheck(parse_path(&fakejob, &longname));
      cpcheck(protocol->parseattrs(&fakejob, attrs));
      attrs->name = name;
      attrs->longname = longname;
      attrs->wname = convertm2w(attrs->name);
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

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_CLOSE);
  send_uint32(&fakeworker, id = newid());
  send_bytes(&fakeworker, hp->data, hp->len);
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_setstat(const char *path,
                        const struct sftpattr *attrs) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_SETSTAT);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  protocol->sendattrs(&fakejob, attrs);
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_fsetstat(const struct handle *hp,
                         const struct sftpattr *attrs) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_FSETSTAT);
  send_uint32(&fakeworker, id = newid());
  send_bytes(&fakeworker, hp->data, hp->len);
  protocol->sendattrs(&fakejob, attrs);
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_rmdir(const char *path) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_RMDIR);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_remove(const char *path) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_REMOVE);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_rename(const char *oldpath, const char *newpath) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_RENAME);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(oldpath));
  send_path(&fakejob, &fakeworker, resolvepath(newpath));
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_symlink(const char *targetpath, const char *linkpath) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_SYMLINK);
  send_uint32(&fakeworker, id = newid());
  if(quirk_openssh && protocol->version == 3) {
    /* OpenSSH server gets SSH_FXP_SYMLINK args back to front
     * - see http://bugzilla.mindrot.org/show_bug.cgi?id=861 */
    send_path(&fakejob, &fakeworker, resolvepath(targetpath));
    send_path(&fakejob, &fakeworker, resolvepath(linkpath));
  } else {
    send_path(&fakejob, &fakeworker, resolvepath(linkpath));
    send_path(&fakejob, &fakeworker, resolvepath(targetpath));
  }
  send_end(&fakeworker);
  getresponse(SSH_FXP_STATUS, id);
  return status();
}

static int sftp_open(const char *path, uint32_t flags, 
                     const struct sftpattr *attrs,
                     struct handle *hp) {
  uint32_t id;

  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_OPEN);
  send_uint32(&fakeworker, id = newid());
  send_path(&fakejob, &fakeworker, resolvepath(path));
  /* For the time being we just send the v3 values.  We'll have to do somethnig
   * cleverer when we support later protocol version. */
  send_uint32(&fakeworker, flags);
  protocol->sendattrs(&fakejob, attrs);
  send_end(&fakeworker);
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
  size_t nattrs, nallattrs = 0, n, m, i, maxnamewidth = 0;
  struct handle h;
  size_t cols, rows;

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
    const int include_dotfiles = !!strchr(options, 'a');

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
                                 : 0)|FORMAT_PREFER_LOCALTIME;
    
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
    if(!cols) cols = 1;
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
      printf("\n");
    }
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
      send_begin(&fakeworker);
      send_uint8(&fakeworker, SSH_FXP_READ);
      send_uint32(&fakeworker, id);
      send_bytes(&fakeworker, r->h.data, r->h.len);
      send_uint64(&fakeworker, r->next_offset);
      if(r->size - r->next_offset > buffersize)
        len = buffersize;
      else {
        len = (uint32_t)(r->size - r->next_offset);
        r->eof = 1;
      }
      send_uint32(&fakeworker, len);
      /* Don't hold the lock while doing the send itself */
      ferrcheck(pthread_mutex_unlock(&r->m));
      send_end(&fakeworker);
      ferrcheck(pthread_mutex_lock(&r->m));
      /* Only fill in the outstanding_read once we've sent it */
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

/* Inbound text file translation */
static FILE *translated_fp;
static size_t translated_state;         /* how far thru newline we've seen */

static int write_translated_init(int fd) {
  if(!(translated_fp = fdopen(fd, "w")))
    return error("error calling fdopen: %s", strerror(errno));
  translated_state = 0;
  return 0;
}

static int write_translated(const void *vptr, size_t bytes) {
  const char *ptr = vptr;
  while(bytes > 0) {
    const int c = (unsigned char)*ptr++;
    --bytes;
    if(c == newline[translated_state]) {
      /* We've seen part of a newline */
      ++translated_state;
      if(!newline[translated_state]) {
        /* We must have seen a whole newline */
        if(putc('\n', translated_fp) < 0) return -1;
        translated_state = 0;
      } else {
        /* We're part way thru something that might be a newline.  Keep
         * going. */
      }
    } else {
      if(translated_state) {
        /* We're part way thru something that turned out not to be a newline. */
        if(fwrite(newline, 1, translated_state, translated_fp)
           != translated_state)
          return -1;
        translated_state = 0;
        /* Try again from the current point. */
        /* Note that we assume that the newline sequence doesn't contain
         * repetitions.  If you have a (completely bonkers!) platform that
         * violates this assumption then you'll have to write a cleverer state
         * machine here. */
        continue;
      } else {
        if(putc(c, translated_fp) < 0) return -1;
      }
    }
  }
  return 0;
}

static int write_translated_done(void) {
  int rc = 0;

  if(translated_fp) {
    if(translated_state) {
      /* The file ends part way thru something that starts out like a newline
       * sequence but turns out not to be one. */
      if(fwrite(newline, 1, translated_state, translated_fp) != translated_state)
        rc = -1;
      translated_state = 0;
    }
    if(fclose(translated_fp) < 0)
      rc = -1;
    translated_fp = 0;
  }
  return rc;
}

static int cmd_get(int ac,
                   char **av) {
  int preserve = 0;
  const char *local, *e;
  char *remote, *tmp = 0;
  struct reader_data r;
  struct sftpattr attrs;
  int fd = -1, n, rc;
  uint8_t rtype;
  uint32_t st, len;
  pthread_t tid;
  uint64_t written = 0;
  struct timeval started, finished;
  double elapsed;

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
  if(textmode) {
    if(write_translated_init(fd))
      goto error;
    fd = -1;
  }
  /* open the remote file */
  if(sftp_open(remote, SSH_FXF_READ|(textmode ? SSH_FXF_TEXT : 0),
               &attrs, &r.h))
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
  gettimeofday(&started,  0);
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
       * the response. 
       *
       * In text mode we can rely on the responses arriving in the correct
       * order.
       */
      if(textmode) {
        /* We must replace each instance of the newline string with \n */
        rc = write_translated(fakejob.ptr, len);
      } else
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
  gettimeofday(&finished, 0);
  if(progress_indicators) {
    elapsed = ((finished.tv_sec - started.tv_sec)
               + (finished.tv_usec - started.tv_usec) / 1000000.0);
    xprintf("%"PRIu64" bytes in %.1f seconds", written, elapsed);
    if(elapsed > 0.1)
      xprintf(" %.0f bytes/sec", written / elapsed);
    xprintf("\n");
  }
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
  if(textmode) {
    if(write_translated_done()) {
      error("error writing to %s: %s", tmp, strerror(errno));
      goto error;
    }
  } else if(close(fd) < 0) {
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
  write_translated_done();              /* ok to call if not initialized */
  if(fd >= 0) close(fd);
  if(tmp) unlink(tmp);
  if(r.h.len) sftp_close(&r.h);
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
    getresponse(SSH_FXP_STATUS, 0/*don't care about id*/);
    ferrcheck(pthread_cond_signal(&w->c2));
    /* Find the request ID */
    for(i = 0; i < nrequests && w->reqs[i].id != fakejob.id; ++i)
      ;
    assert(i < nrequests);
    --w->outstanding;
    cpcheck(parse_uint32(&fakejob, &st));
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
  int fd = -1, i, preserve = 0, failed = 0,  eof = 0;
  struct handle h;
  off_t offset;
  ssize_t n;
  struct writer_data w;
  pthread_t tid;
  struct timeval started, finished;
  double elapsed;
  uint32_t id;
  FILE *fp = 0;

  memset(&h, 0, sizeof h);
  memset(&attrs, 0, sizeof attrs);
  memset(&w, 0, sizeof w);
  if(!strcmp(*av, "-P")) {
    preserve = 1;
    ++av;
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
    stat_to_attrs(fakejob.a, &sb, &attrs, 0xFFFFFFFF, local);
    /* Mask out things that don't make sense: we set the size by dint of
     * uploading data, we don't want to try to set a numeric UID or GID, and we
     * cannot set the allocation size or link count. */
    attrs.valid &= ~(SSH_FILEXFER_ATTR_SIZE
                     |SSH_FILEXFER_ATTR_LINK_COUNT
                     |SSH_FILEXFER_ATTR_UIDGID);
    attrs.attrib_bits &= ~SSH_FILEXFER_ATTR_FLAGS_HIDDEN;
  }
  if(sftp_open(remote, (SSH_FXF_WRITE|SSH_FXF_CREAT|SSH_FXF_TRUNC
                        |(textmode ? SSH_FXF_TEXT : 0)),
               &attrs, &h))
    goto error;
  if(textmode) {
    if(!(fp = fdopen(fd, "r"))) {
      error("error calling fdopen: %s", strerror(errno));
      goto error;
    }
    fd = -1;
  }
  w.reqs = alloc(fakejob.a, nrequests * sizeof *w.reqs);
  w.remote = remote;
  gettimeofday(&started,  0);
  ferrcheck(pthread_mutex_init(&w.m, 0));
  ferrcheck(pthread_cond_init(&w.c1, 0));
  ferrcheck(pthread_cond_init(&w.c2, 0));
  ferrcheck(pthread_create(&tid, 0, writer_thread, &w));
  ferrcheck(pthread_mutex_lock(&w.m));
  offset = 0;
  while(!w.failed && !eof && !failed) {
    /* Wait until we're allowed to send another request */
    if(w.outstanding >= nrequests) {
      ferrcheck(pthread_cond_wait(&w.c2, &w.m));
      continue;
    }
    /* Release the lock while we mess around with IO */
    ferrcheck(pthread_mutex_unlock(&w.m));
    /* Construct a write command */
    send_begin(&fakeworker);
    send_uint8(&fakeworker, SSH_FXP_WRITE);
    send_uint32(&fakeworker, id = newid());
    send_bytes(&fakeworker, h.data, h.len);
    send_uint64(&fakeworker, offset);
    send_need(fakejob.worker, buffersize + 4);
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
            if(ungetc(c, fp) < 0) fatal("ungetc: %s", strerror(errno));
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
      /* Send off another write request with however much data we read */
      send_uint32(&fakeworker, n);
      fakejob.worker->bufused += n;
      send_end(&fakeworker);
      offset += n;
      /* We can only fill in the request details when we have the lock, so we
       * don't do that yet */
    } else if(n == 0)
      /* We reached EOF on the input file */
      eof = 1;
    else {
      /* Error reading the input file */
      error("error reading %s: %s", local, strerror(errno));
      failed = 1;
    }
    ferrcheck(pthread_mutex_lock(&w.m));
    if(!(eof || failed)) {
      for(i = 0; i < nrequests && w.reqs[i].id; ++i)
        ;
      assert(i < nrequests);
      w.reqs[i].id = id;
      w.reqs[i].n = n;
      ++w.outstanding;
    }
    ferrcheck(pthread_cond_signal(&w.c1));
  }
  w.finished = 1;
  ferrcheck(pthread_cond_signal(&w.c1));
  ferrcheck(pthread_mutex_unlock(&w.m));
  ferrcheck(pthread_join(tid, 0));
  ferrcheck(pthread_mutex_destroy(&w.m));
  ferrcheck(pthread_cond_destroy(&w.c1));
  ferrcheck(pthread_cond_destroy(&w.c2));
  if(failed || w.failed) goto error;
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
    if(sftp_fsetstat(&h, &attrs)) goto error;
  }
  sftp_close(&h);
  return 0;
error:
  if(fp) fclose(fp);
  if(fd >= 0) close(fd);
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

static int cmd_version(int attribute((unused)) ac,
                       char attribute((unused)) **av) {
  xprintf("%d\n", protocol->version);
  return 0;
}

/* Table of command line operations */
static const struct command commands[] = {
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
    "exit", 0, 0, cmd_quit,
    0,
    "quit"
  },
  {
    "get", 1, 3, cmd_get,
    "[-P] REMOTE-PATH [LOCAL-PATH]",
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
    "progress", 0, 1, cmd_progress,
    "[on|off]",
    "set or toggle progress indicators"
  },
  {
    "put", 1, 3, cmd_put, "[-P] LOCAL-PATH [REMOTE-PATH]",
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
    "text", 0, 0, cmd_text,
    0,
    "text mode"
  },
  {
    "version", 0, 0, cmd_version,
    0,
    "display protocol version"
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

  while((n = getopt_long(argc, argv, "hVB:b:P:R:s:S:12CF:o:vd",
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
    case 'd': debugging = 1; break;
    case 256: quirk_openssh = 1; break;
    default: exit(1);
    }
  }

  /* sanity checking */
  if(nrequests <= 0) nrequests = 1;
  if(nrequests > 128) nrequests = 128;
  if(buffersize < 64) buffersize = 64;
  if(buffersize > 1048576) buffersize = 1048576;

  if(sftpversion < 3 || sftpversion > 4)
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
  if((fakeworker.utf8_to_local = iconv_open(nl_langinfo(CODESET), "UTF-8"))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));
  if((fakeworker.local_to_utf8 = iconv_open("UTF-8", nl_langinfo(CODESET)))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));

  /* Send SSH_FXP_INIT */
  send_begin(&fakeworker);
  send_uint8(&fakeworker, SSH_FXP_INIT);
  send_uint32(&fakeworker, sftpversion);
  send_end(&fakeworker);
  
  /* Parse the version reponse */
  getresponse(SSH_FXP_VERSION, 0);
  cpcheck(parse_uint32(&fakejob, &u32));
  switch(u32) {
  case 3:
    protocol = &sftpv3;
    break;
  case 4:
    protocol = &sftpv4;
    break;
  default:
    fatal("server wanted protocol version %"PRIu32, u32);
  }
  /* Extension data */
  while(fakejob.left) {
    char *xname, *xdata;

    cpcheck(parse_string(&fakejob, &xname, 0));
    cpcheck(parse_string(&fakejob, &xdata, 0));
    D(("server sent extension '%s'", xname));
    if(!strcmp(xname, "newline")) {
      newline = xstrdup(xdata);
      if(!*newline)
        fatal("cannot cope with empty newline sequence");
      /* TODO check newline sequence doesn't contain repeats */
    }
  }
  /* Make sure outbound translation will actually work */
  if(buffersize < strlen(newline))
    buffersize = strlen(newline);

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
