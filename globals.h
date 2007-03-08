#ifndef GLOBALS_H
#define GLOBALS_H

extern struct queue *workqueue;
/* Queue to which jobs are sent */

extern const struct sftpprotocol sftpv6, sftpv5, sftpv4, sftpv3, sftppreinit;
extern const struct sftpprotocol *protocol;
extern const char sendtype[];
extern int readonly;

#endif /* GLOBALS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
