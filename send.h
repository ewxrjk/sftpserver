#ifndef SEND_H
#define SEND_H

void send_need(struct worker *w, size_t n);
/* Make sure there are N bytes available in the buffer */

void send_begin(struct worker *w);
void send_end(struct worker *w);
void send_uint8(struct worker *w, int n);
void send_uint16(struct worker *w, uint16_t u);
void send_uint32(struct worker *w, uint32_t u);
void send_uint64(struct worker *w, uint64_t u);
void send_bytes(struct worker *w, const void *bytes, size_t n);
void send_handle(struct worker *w, const struct handleid *id);
#define send_string(JOB, S) do {                \
  const char *s_ = (S);                         \
  send_bytes(JOB, s_, strlen(s_));              \
} while(0)
void send_path(struct sftpjob *job, struct worker *w, const char *path);

size_t send_sub_begin(struct worker *w);
void send_sub_end(struct worker *w, size_t offset);

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
