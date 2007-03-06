#ifndef STAT_H
#define STAT_H

const char *format_attr(struct allocator *a,
			const struct sftpattr *attrs, int thisyear,
			unsigned long flags);
#define FORMAT_PREFER_NUMERIC_UID 0x00000001
#define FORMAT_PREFER_LOCALTIME 0x00000002
/* Prefer numeric UID instead of names */

void normalize_ownergroup(struct allocator *a,
                          struct sftpattr *attrs);

const char *set_status(struct allocator *a,
                       const char *path,
		       const struct sftpattr *attrs);
const char *set_fstatus(struct allocator *a,
                        int fd,
			const struct sftpattr *attrs);
void stat_to_attrs(struct allocator *a,
		   const struct stat *sb, struct sftpattr *attrs,
                   uint32_t flags, const char *path);

#endif /* STAT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
