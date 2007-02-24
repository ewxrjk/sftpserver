#ifndef PARSE_H
#define PARSE_H

int parse_uint32(struct sftpjob *job, uint32_t *ur);
int parse_uint64(struct sftpjob *job, uint64_t *ur);
int parse_string(struct sftpjob *job, char **strp, size_t *lenp);
int parse_path(struct sftpjob *job, char **strp);
int parse_handle(struct sftpjob *job, struct handleid *id);
/* Parse various values out of the remaining data of a job.  Return 0 on
 * success, non-0 on error. */

#define pcheck(E) do {					\
  if((E)) {						\
    D(("%s:%d: %s", __FILE__, __LINE__, #E));		\
    protocol->status(job, SSH_FX_BAD_MESSAGE, 0);	\
    return;						\
  }							\
} while(0)
/* error-checking wrapper for parse_ functions */

#endif /* PARSE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
