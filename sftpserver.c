#include "sftpserver.h"
#include "queue.h"
#include "alloc.h"
#include "debug.h"
#include "utils.h"
#include "sftp.h"
#include "send.h"
#include "parse.h"
#include "types.h"
#include "globals.h"
#include "serialize.h"
#include "xfns.h"
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#include <langinfo.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <stdio.h>

/* Forward declarations */

static void *worker_init(void);
static void worker_cleanup(void *wdv);
static void process_sftpjob(void *jv, void *wdv, struct allocator *a);
static void sftp_service(void);

/* Globals */

const char *local_encoding;
struct queue *workqueue;

static const struct queuedetails workqueue_details = {
  worker_init,
  process_sftpjob,
  worker_cleanup
};

const struct sftpprotocol *protocol = &sftppreinit;
const char sendtype[] = "response";

/* Options */

static const struct option options[] = {
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'V' },
  { "debug", no_argument, 0, 'd' },
  { "debug-file", required_argument, 0, 'D' },
  { "chroot", required_argument, 0, 'r' },
  { "user", required_argument, 0, 'u' },
  { "listen", required_argument, 0, 'L' },
  { "host", required_argument, 0, 'H' },
  { "background", no_argument, 0, 'b' },
  { "ipv4", no_argument, 0, '4' },
  { "ipv6", no_argument, 0, '6' },
  { 0, 0, 0, 0 }
};

/* display usage message and terminate */
static void help(void) {
  xprintf("Usage:\n"
          "  gesftpserver [OPTIONS]\n"
          "\n"
          "Green End SFTP server.  Not intended for interactive use!\n"
          "\n"
          "Options:\n"
          "  --help, -h               Display usage message\n"
          "  --version, -V            Display version number\n"
          "  --chroot, -r PATH        Change root to PATH\n"
          "  --user, -u USER          Change to user USER\n"
          "  --listen, -L PORT        Listen on PORT\n"
          "  --host, -H HOSTNAME      Bind to HOSTNAME (default *)\n"
          "  -4|-6                    Force IPv4 or IPv6 for --liisten\n"
          "  --background, -b         Daemonize\n");
  exit(0);
}

/* display version number and terminate */
static void version(void) {
  xprintf("Green End SFTP server version %s\n", VERSION);
  exit(0);
}

/* Initialization */

static void sftp_init(struct sftpjob *job) {
  uint32_t version;

  if(protocol != &sftppreinit) {
    /* Cannot initialize more than once */
    send_status(job, SSH_FX_FAILURE, "already initialized");
    return;
  }
  if(parse_uint32(job, &version)) {
    send_status(job, SSH_FX_BAD_MESSAGE, "no version found in SSH_FXP_INIT");
    return;
  }
  switch(version) {
  case 0: case 1: case 2:
    send_status(job, SSH_FX_OP_UNSUPPORTED,
                "client protocol version is too old (need at least 3)");
    return;
  case 3:
    protocol = &sftpv3;
    break;
  case 4:
    protocol = &sftpv4;
    break;
  case 5:
    protocol = &sftpv5;
    break;
  default:
    protocol = &sftpv6;
    break;
  }
  send_begin(job->worker);
  send_uint8(job->worker, SSH_FXP_VERSION);
  send_uint32(job->worker, protocol->version);
  if(protocol->version >= 4) {
    /* e.g. draft-ietf-secsh-filexfer-04.txt, 4.3.  This allows us to assume the
     * client always sends \n, freeing us from the burden of translating text
     * files.  However we still have to deal with the different rules for reads
     * and writes on text files.
     */
    send_string(job->worker, "newline");
    send_string(job->worker, "\n");
  }
  if(protocol->version == 5) {
    /* draft-ietf-secsh-filexfer-05.txt 4.4 */
    send_string(job->worker, "supported");
    const size_t offset = send_sub_begin(job->worker);
    send_uint32(job->worker, (SSH_FILEXFER_ATTR_SIZE
                              |SSH_FILEXFER_ATTR_PERMISSIONS
                              |SSH_FILEXFER_ATTR_ACCESSTIME
                              |SSH_FILEXFER_ATTR_MODIFYTIME
                              |SSH_FILEXFER_ATTR_OWNERGROUP
                              |SSH_FILEXFER_ATTR_SUBSECOND_TIMES));
    send_uint32(job->worker, 0);         /* supported-attribute-bits */
    send_uint32(job->worker, (SSH_FXF_ACCESS_DISPOSITION
                              |SSH_FXF_APPEND_DATA
                              |SSH_FXF_APPEND_DATA_ATOMIC
                              |SSH_FXF_TEXT_MODE));
    send_uint32(job->worker, 0xFFFFFFFF);
    /* If we send a non-0 max-read-size then we promise to return that many
     * bytes if asked for it and to mean EOF or error if we return less.
     *
     * This is completely useless.  If we end up reading from something like a
     * pipe then we may get a short read before EOF.  If we've sent a non-0
     * max-read-size then the client will wrongly interpret this as EOF.
     *
     * Therefore we send 0 here.
     */
    send_uint32(job->worker, 0);
    send_string(job->worker, "space-available");
    send_sub_end(job->worker, offset);
  }
  if(protocol->version >= 6) {
    /* draft-ietf-secsh-filexfer-13.txt 5.4 */
    send_string(job->worker, "supported2");
    const size_t offset = send_sub_begin(job->worker);
    send_uint32(job->worker, (SSH_FILEXFER_ATTR_SIZE
                              |SSH_FILEXFER_ATTR_PERMISSIONS
                              |SSH_FILEXFER_ATTR_ACCESSTIME
                              |SSH_FILEXFER_ATTR_MODIFYTIME
                              |SSH_FILEXFER_ATTR_OWNERGROUP
                              |SSH_FILEXFER_ATTR_SUBSECOND_TIMES
                              |SSH_FILEXFER_ATTR_CTIME
                              |SSH_FILEXFER_ATTR_LINK_COUNT));
    send_uint32(job->worker, 0);         /* supported-attribute-bits */
    send_uint32(job->worker, (SSH_FXF_ACCESS_DISPOSITION
                              |SSH_FXF_APPEND_DATA
                              |SSH_FXF_APPEND_DATA_ATOMIC
                              |SSH_FXF_TEXT_MODE
                              |SSH_FXF_NOFOLLOW
                              |SSH_FXF_DELETE_ON_CLOSE));
    send_uint32(job->worker, 0xFFFFFFFF);
    send_uint32(job->worker, 0);        /* max-read-size - see above */
    send_uint16(job->worker, 0);        /* supported-open-block-vector */
    send_uint16(job->worker, 0);        /* supported-block-vector */
    send_uint32(job->worker, 0);        /* attrib-extension-count */
    send_uint32(job->worker, 1);        /* extension-count */
    send_string(job->worker, "space-available");
    send_sub_end(job->worker, offset);
    /* e.g. draft-ietf-secsh-filexfer-13.txt, 5.5 */
    send_string(job->worker, "versions");
    send_string(job->worker, "3,4,5,6");
  }
  {
    /* vendor-id is defined in some of the SFTP drafts but not all.
     * Whatever. */
    send_string(job->worker, "vendor-id");
    const size_t offset = send_sub_begin(job->worker);
    send_string(job->worker, "Green End");
    send_string(job->worker, "Green End SFTP Server");
    send_string(job->worker, VERSION);
    send_uint64(job->worker, 0);
    send_sub_end(job->worker, offset);
  }
  send_end(job->worker);
  /* Now we are initialized we can safely process other jobs in the
   * background. */
  queue_init(&workqueue, &workqueue_details, 4);
}

static const struct sftpcmd sftppreinittab[] = {
  { SSH_FXP_INIT, sftp_init }
};

const struct sftpprotocol sftppreinit = {
  sizeof sftppreinittab / sizeof (struct sftpcmd),
  sftppreinittab,
  3,
  0xFFFFFFFF,                           /* never used */
  SSH_FX_OP_UNSUPPORTED,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

/* Worker setup/teardown */

static void *worker_init(void) {
  struct worker *w = xmalloc(sizeof *w);

  memset(w, 0, sizeof *w);
  w->buffer = 0;
  if((w->utf8_to_local = iconv_open(local_encoding, "UTF-8"))
     == (iconv_t)-1)
    fatal("error calling iconv_open(%s,UTF-8): %s",
          local_encoding, strerror(errno));
  if((w->local_to_utf8 = iconv_open("UTF-8", local_encoding))
     == (iconv_t)-1)
    fatal("error calling iconv_open(UTF-8,%s): %s",
          local_encoding, strerror(errno));
  return w;
}

static void worker_cleanup(void *wdv) {
  struct worker *w = wdv;

  free(w->buffer);
  free(w);
}

/* Main loop */

/* Process a job */
static void process_sftpjob(void *jv, void *wdv, struct allocator *a) {
  struct sftpjob *const job = jv;
  int l, r, type;
  
  job->a = a;
  job->id = 0;
  job->worker = wdv;
  job->ptr = job->data;
  job->left = job->len;
  /* Empty messages are never valid */
  if(!job->left) {
    send_status(job, SSH_FX_BAD_MESSAGE, "empty request");
    goto done;
  }
  /* Get the type */
  type = *job->ptr++;
  --job->left;
  /* Everything but SSH_FXP_INIT has an ID field */
  if(type != SSH_FXP_INIT)
    if(parse_uint32(job, &job->id)) {
      send_status(job, SSH_FX_BAD_MESSAGE, "missing ID field");
      goto done;
    }
  /* Locate the handler for the command */
  l = 0;
  r = protocol->ncommands - 1;
  while(l <= r) {
    const int m = (l + r) / 2;
    const int mtype = protocol->commands[m].type;
    
    if(type < mtype) r = m - 1;
    else if(type > mtype) l = m + 1;
    else {
      /* Serialize */
      serialize(job);
      /* Run the handler */
      protocol->commands[m].handler(job);
      goto done;
    }
  }
  /* We did not find a handler */
  send_status(job, SSH_FX_OP_UNSUPPORTED, "operation not supported");
done:
  serialize_remove_job(job);
  free(job->data);
  free(job);
  return;
}

static void sigchld_handler(int attribute((unused)) sig) {
  const int save_errno = errno;
  int w;

  while(waitpid(-1, &w, WNOHANG) > 0)
    ;
  errno = save_errno;
}

int main(int argc, char **argv) {
  int n, listenfd = -1;
  const char *root = 0, *user = 0;
  struct passwd *pw = 0;
  const char *host = 0, *port = 0;
  int daemonize = 0;
  struct addrinfo hints;
  iconv_t cd;

  memset(&hints, 0, sizeof hints);
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* We need I18N support for filename encoding */
  setlocale(LC_CTYPE, "");
  local_encoding = nl_langinfo(CODESET);
  
  while((n = getopt_long(argc, argv, "hVdD:r:u:H:L:b46",
			 options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    case 'D': debugging = 1; debugpath = optarg; break;
    case 'r': root = optarg; break;
    case 'u': user = optarg; break;
    case 'H': host = optarg; break;
    case 'L': port = optarg; break;
    case 'b': daemonize = 1; break;
    case '4': hints.ai_family = PF_INET; break;
    case '6': hints.ai_family = PF_INET6; break;
    default: exit(1);
    }
  }

  if(daemonize && !port)
    fatal("--background requires --port");

  /* If writes to the client fail then we'll get EPIPE.  Arguably it might
   * better just to die the SIGPIPE but reporting an EPIPE is pretty harmless.
   *
   * If by some chance we end up writing to a pipe then we'd rather have an
   * EPIPE so we can report it back to the client than a SIGPIPE which will
   * (from the client's POV) cause us to close the connection without
   * responding to at least one command.
   *
   * Therefore, we ignore SIGPIPE.
   *
   *
   * As for other signals, we assume that if someone invokes us with an unusual
   * signal disposition, they have a good reason for it.
   */
  signal(SIGPIPE, SIG_IGN);

  /* Enable debugging */
  if(getenv("SFTPSERVER_DEBUGGING"))
    debugging = 1;

  if(user) {
    /* Look up the user */
    if(!(pw = getpwnam(user)))
      fatal("no such user as %s", user);
    if(initgroups(user, pw->pw_gid))
      fatal("error calling initgroups: %s", strerror(errno));
  }
  
  if(port) {
    struct addrinfo *res;
    int rc;
    static const int one = 1;
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGCHLD, &sa, 0) < 0)
      fatal("error calling sigaction: %s", strerror(errno));
    if((rc = getaddrinfo(host, port, &hints, &res))) {
      if(host)
        fatal("error resolving host %s port %s: %s",
              host, port, gai_strerror(rc));
      else
        fatal("error resolving port %s: %s",
              port, gai_strerror(rc));
    }
    if((listenfd = socket(res->ai_family, res->ai_socktype,
                          res->ai_protocol)) < 0)
      fatal("error calling socket: %s", strerror(errno));
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one,  sizeof one) < 0)
      fatal("error calling setsockopt: %s", strerror(errno));
    if(bind(listenfd, res->ai_addr, res->ai_addrlen) < 0)
      fatal("error calling socket: %s", strerror(errno));
    if(listen(listenfd, SOMAXCONN) < 0)
      fatal("error calling listen: %s", strerror(errno));
  } else if(host)
    fatal("--host makes no sense without --port");

  if((cd = iconv_open(local_encoding, "UTF-8")) == (iconv_t)-1)
    fatal("error calling iconv_open(%s,UTF-8): %s",
          local_encoding, strerror(errno));
  iconv_close(cd);
  if((cd = iconv_open("UTF-8", local_encoding)) == (iconv_t)-1)
    fatal("error calling iconv_open(UTF-8, %s): %s",
          local_encoding, strerror(errno));
  iconv_close(cd);
  
  if(root) {
    /* Enter our chroot */
    if(chdir(root) < 0)
      fatal("error calling chdir %s: %s", root, strerror(errno));
    if(chroot(".") < 0)
      fatal("error calling chroot: %s", strerror(errno));
  }

  if(user) {
    /* Become the right user */
    assert(pw != 0);
    if(setgid(pw->pw_gid) < 0)
      fatal("error calling setgid: %s", strerror(errno));
    if(setuid(pw->pw_uid) < 0)
      fatal("error calling setuid: %s", strerror(errno));
    if(setuid(0) >= 0)
      fatal("setuid(0) unexpectedly succeeded");
  }
  
  if(daemonize)
    if(daemon(0, 0) < 0)
      fatal("error calling daemon: %s", strerror(errno));
  /* TODO logging */

  if(!port) {
    sftp_service();
    return 0;
  } else {
    for(;;) {
      union {
        struct sockaddr_in sin;
        struct sockaddr_in6 sin6;
        struct sockaddr sa;
      } addr;
      socklen_t addrlen = sizeof addr;
      int fd;
    
      if((fd = accept(listenfd, &addr.sa, &addrlen)) >= 0) {
        switch(fork()) {
        case -1:
          /* If we can't fork then we stop trying for a minute */
          fprintf(stderr, "fork: %s\n", strerror(errno));
          close(fd);
          sleep(60);
          break;
        case 0:
          forked();
          signal(SIGCHLD, SIG_DFL);       /* XXX */
          if(dup2(fd, 0) < 0
             || dup2(fd, 1) < 0)
            fatal("dup2: %s", strerror(errno));
          if(close(fd) < 0
             || close(listenfd) < 0)
            fatal("close: %s", strerror(errno));
          sftp_service();
          _exit(0);
        default:
          close(fd);
          break;
        }
      }
    }
  }
}

static void sftp_service(void) {
  uint32_t len;
  struct sftpjob *job;
  struct allocator a;
  void *const wdv = worker_init(); 
  D(("gesftpserver %s starting up", VERSION));
  /* draft -13 s7.6 "The server SHOULD NOT apply a 'umask' to the mode
   * bits". */
  umask(0);
  while(!do_read(0, &len, sizeof len)) {
    job = xmalloc(sizeof *job);
    job->len = ntohl(len);
    if(!job->len)
      fatal("zero length job");         /* that's not cricket */
    job->data = xmalloc(job->len);
    if(do_read(0, job->data, job->len))
      /* Job data missing or truncated - the other end is not playing the game
       * fair so we give up straight away */
      fatal("read error: unexpected eof");
    if(debugging) {
      D(("request:"));
      hexdump(job->data, job->len);
    }
    /* See serialize.c for the serialization rules we follow */
    queue_serializable_job(job);
    /* We process the job in a background thread, except that the background
     * threads don't exist until SSH_FXP_INIT has succeeded. */
    if(workqueue)
      queue_add(workqueue, job);
    else {
      alloc_init(&a);
      process_sftpjob(job, wdv, &a);
      alloc_destroy(&a);
    }
    /* process_sftpjob() frees JOB when it has finished with it */
  }
  queue_destroy(workqueue);
  worker_cleanup(wdv);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
