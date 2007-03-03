bin_PROGRAMS=sftpserver sftpclient
noinst_LIBRARIES=libsftp.a

sftpserver_SOURCES=sftpserver.c
sftpserver_LDADD=libsftp.a

sftpclient_SOURCES=sftpclient.c
sftpclient_LDADD=libsftp.a

libsftp_a_SOURCES=alloc.c alloc.h debug.c debug.h globals.h handle.c	\
handle.h parse.c parse.h queue.c queue.h send.c send.h sftp.h		\
sftpclient.h sftpcommon.h sftpserver.h status.c thread.h types.h	\
users.c users.h utils.c utils.h v3.c xfns.c xfns.h stat.c charset.c	\
charset.h serialize.h serialize.c v4.c
