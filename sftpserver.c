#include "sftpserver.h"
#include "queue.h"
#include "alloc.h"
#include "debug.h"

/* Forward declarations */

static void *worker_init(void);
static void worker_cleanup(void *wdv);
static void process_sftpjob(void *jv, void *wdv, struct allocator *a);

/* Globals */

#if HAVE_PTHREAD_H
struct queue *workqueue;

static const struct queuedetails workqueue_details = {
  worker_init,
  process_sftpjob,
  worker_cleanup
};
#endif

const struct sftpprotocol *protocol = &sftppreinit;

/* Initialization */

static void sftp_init(struct sftpjob *job) {
  uint32_t version;

  if(protocol != &sftppreinit) {
    /* Cannot initialize more than once */
    protocol->status(job, SSH_FX_FAILURE, "already initialized");
    return;
  }
  if(parse_uint32(job, &version)) {
    protocol->status(job, SSH_FX_BAD_MESSAGE,
                     "no version found in SSH_FXP_INIT");
    return;
  }
  switch(version) {
  case 0: case 1: case 2:               /* we don't understand these at all. */
    protocol->status(job, SSH_FX_OP_UNSUPPORTED,
                     "client protocol version is too old (need at least 3)");
    return;
  case 3:
  case 4:
    /* If the client offers v3 then it might be sending extension data.  We
     * would parse it here if we care, but right now we don't know how to
     * support any extensions the client might ask for that way. */
    break;
  default:
    version = 4;                        /* highest we know right now */
    break;
  }
  switch(version) {
  case 3:
    protocol = &sftpv3;
    break;
  default:
    assert(!"cannot happen");
  }
  send_begin(job);
  send_uint8(job, SSH_FXP_VERSION);
  send_uint32(job, version);
  if(version >= 6) {                    /* doesn't make much sense yet! */
    /* e.g. draft-ietf-secsh-filexfer-13.txt, 5.5 */
    send_string(job, "versions");
    send_string(job, "3");
  }
  /* TODO filename-charset extension */
  /* TODO supported extension */
  /* TODO supported2 extension */
  send_end(job);
#if HAVE_PTHREAD_H
  /* Now we are initialized we can safely process other jobs in the
   * background. */
  queue_init(&workqueue, &workqueue_details, 4);
#endif
}

static const struct sftpcmd sftppreinittab[] = {
  { SSH_FXP_INIT, sftp_init }
};

const struct sftpprotocol sftppreinit = {
  sizeof sftppreinittab / sizeof (struct sftpcmd),
  sftppreinittab,
  v3_status,
  v3_sendnames,
  v3_sendattrs,
  v3_encode,
  v3_decode
};

/* Worker setup/teardown */

static void *worker_init(void) {
  struct worker *w = xmalloc(sizeof *w);

  memset(w, 0, sizeof *w);
  w->buffer = 0;
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
    protocol->status(job, SSH_FX_BAD_MESSAGE, "empty request");
    goto done;
  }
  /* Get the type */
  type = *job->ptr++;
  --job->left;
  /* Everything but SSH_FXP_INIT has an ID field */
  if(type != SSH_FXP_INIT)
    if(parse_uint32(job, &job->id)) {
      protocol->status(job, SSH_FX_BAD_MESSAGE, "missing ID field");
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
  protocol->status(job, SSH_FX_OP_UNSUPPORTED, "operation not supported");
done:
  free(job->data);
  free(job);
  return;
}

int main(void) {
  uint32_t len;
  struct sftpjob *job;
  struct allocator a;
  void *const wdv = worker_init(); 

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
#ifdef DEBUGPATH
  debugfp = fopen(DEBUGPATH, "w");
#endif
  while(do_fread(&len, sizeof len, 1, stdin)) {
    job = xmalloc(sizeof *job);
    job->len = ntohl(len);
    job->data = xmalloc(len);
    if(!do_fread(job->data, 1, job->len, stdin)) {
      /* Job data missing or truncated - the other end is not playing the game
       * fair so we give up straight away */
      fprintf(stderr, "read error: unexpected eof\n");
      exit(-1);
    }
    if(debugfp) {
      fprintf(debugfp, "request:\n");
      hexdump(debugfp, job->data, job->len);
      fputc('\n', debugfp);
    }
    /* For threaded systems we process the job in a background thread, except
     * that the background threads don't exist until SSH_FXP_INIT has
     * succeeded.  For nonthreaded systems we must always process the job in
     * the foreground. */
#if HAVE_PTHREAD_H
    if(workqueue)
      queue_add(workqueue, job);
    else
#endif
    {
      alloc_init(&a);
      process_sftpjob(job, wdv, &a);
      alloc_destroy(&a);
    }
    /* process_sftpjob() frees JOB when it has finished with it */
  }
#if HAVE_PTHREAD_H
  queue_destroy(workqueue);
#endif
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
