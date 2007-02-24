#include "sftpserver.h"
#include "alloc.h"
#include "debug.h"

int parse_uint32(struct sftpjob *job, uint32_t *ur) {
  uint32_t u;

  if(job->left < 4) return -1;
  u = *job->ptr++;
  u = (u << 8) + *job->ptr++;
  u = (u << 8) + *job->ptr++;
  u = (u << 8) + *job->ptr++;
  *ur = u;
  return 0;
}

int parse_string(struct sftpjob *job, char **strp, size_t *lenp) {
  uint32_t len;
  char *str;

  if(parse_uint32(job, &len)) return -1;
  if(!(len + 1)) return -1;             /* overflow */
  str = alloc(job->a, len + 1);         /* 0-fills */
  memcpy(str, job->ptr, len);
  job->ptr += len;
  if(strp) *strp = str;
  if(lenp) *lenp = len;
  return 0;
}

int parse_path(struct sftpjob *job, char **strp) {
  if(parse_string(job, strp, 0)) return -1;
  return protocol->decode(job, strp);
}

int parse_handle(struct sftpjob *job, struct handleid *id) {
  uint32_t len;

  if(parse_uint32(job, &len)
     || len != 8
     || parse_uint32(job, &id->id)
     || parse_uint32(job, &id->tag))
    return -1;
  return 0;
}

/*
LOCAL Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
