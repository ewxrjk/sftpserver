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

/* Forward declarations */

static void *worker_init(void);
static void worker_cleanup(void *wdv);
static void process_sftpjob(void *jv, void *wdv, struct allocator *a);

/* Globals */

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
          "  --version, -V            Display version number\n");
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
    send_uint32(job->worker, 0);        /* extension-count */
    send_sub_end(job->worker, offset);
    /* e.g. draft-ietf-secsh-filexfer-13.txt, 5.5 */
    send_string(job->worker, "versions");
    send_string(job->worker, "3,4,5");
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
  if((w->utf8_to_local = iconv_open(nl_langinfo(CODESET), "UTF-8"))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));
  if((w->local_to_utf8 = iconv_open("UTF-8", nl_langinfo(CODESET)))
     == (iconv_t)-1)
    fatal("error calling iconv_open: %s", strerror(errno));
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

int main(int argc, char **argv) {
  uint32_t len;
  struct sftpjob *job;
  struct allocator a;
  void *const wdv = worker_init(); 
  int n;

  while((n = getopt_long(argc, argv, "hVdD:",
			 options, 0)) >= 0) {
    switch(n) {
    case 'h': help();
    case 'V': version();
    case 'd': debugging = 1; break;
    case 'D': debugging = 1; debugpath = optarg; break;
    default: exit(1);
    }
  }

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
  /* We need I18N support for filename encoding */
  setlocale(LC_CTYPE, "");
  /* Enable debugging */
  if(getenv("SFTPSERVER_DEBUGGING"))
    debugging = 1;
  D(("gesftpserver %s starting up", VERSION));
  while(!do_read(0, &len, sizeof len)) {
    job = xmalloc(sizeof *job);
    job->len = ntohl(len);
    job->data = xmalloc(len);
    if(do_read(0, job->data, job->len))
      /* Job data missing or truncated - the other end is not playing the game
       * fair so we give up straight away */
      fatal("read error: unexpected eof");
    if(debugging) {
      D(("request:"));
      hexdump(job->data, job->len);
    }
    /* Overlapping or text-mode reads and writes on the same handle must be
     * processed in the order in which they arrived.  Therefore we must pick
     * out reads and writes and add them to a queue to allow this rule to be
     * implemented.  See handle.c for fuller commentary and notes on
     * interpretation. */
    if(job->len) {
      switch(*job->data) {
      case SSH_FXP_READ:
      case SSH_FXP_WRITE:
      case SSH_FXP_FSETSTAT:
      case SSH_FXP_FSTAT:
        queue_serializable_job(job);
        break;
      }
    }
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
