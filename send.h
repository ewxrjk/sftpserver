#ifndef SEND_H
#define SEND_H

void send_need(struct worker *w, size_t n);
/* Make sure there are N bytes available in the buffer */

void send_begin(struct sftpjob *job);
void send_end(struct sftpjob *job);
void send_uint8(struct sftpjob *job, int n);
void send_uint32(struct sftpjob *job, uint32_t u);
void send_uint64(struct sftpjob *job, uint64_t u);
void send_bytes(struct sftpjob *job, const void *bytes, size_t n);
void send_handle(struct sftpjob *job, const struct handleid *id);
#define send_string(JOB, S) do {                \
  const char *s_ = (S);                         \
  send_bytes(JOB, s_, strlen(s_));              \
} while(0)
void send_path(struct sftpjob *job, const char *path);

extern int sftpout;                     /* fd to write to */

#endif /* SEND_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
