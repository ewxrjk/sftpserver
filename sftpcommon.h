#ifndef SFTPCOMMON_H
#define SFTPCOMMON_H

#include <config.h>
#include <inttypes.h>

struct queue;
struct allocator;
struct handleid;
struct sftpjob;
struct sftpattr;
struct worker;
struct stat;

const char *status_to_string(uint32_t status);

const char *format_attr(struct allocator *a,
			const struct sftpattr *attrs, int thisyear,
			unsigned flags);
#define FORMAT_PREFER_NUMERIC_UID 0x00000001

#endif /* SFTPCOMMON_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
