#ifndef STAT_H
#define STAT_H

const char *format_attr(struct allocator *a,
			const struct sftpattr *attrs, int thisyear,
			unsigned long flags);
#define FORMAT_PREFER_NUMERIC_UID 0x00000001
/* Prefer numeric UID instead of names */

const char *set_status(const char *path,
		       const struct sftpattr *attrs);
const char *set_fstatus(int fd,
			const struct sftpattr *attrs);

#endif /* STAT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
