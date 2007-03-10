noinst_PROGRAMS=sftpclient
noinst_SCRIPTS=gesftpserver-debug
libexec_PROGRAMS=gesftpserver
noinst_LIBRARIES=libsftp.a

gesftpserver_SOURCES=sftpserver.c readwrite.c
gesftpserver_LDADD=libsftp.a

#gesftpserverro_SOURCES=sftpserver.c readonly.c
#gesftpserverro_LDADD=libsftp.a

sftpclient_SOURCES=sftpclient.c readwrite.c
sftpclient_LDADD=libsftp.a $(LIBREADLINE)

libsftp_a_SOURCES=alloc.c alloc.h debug.c debug.h globals.h handle.c	\
handle.h parse.c parse.h queue.c queue.h send.c send.h sftp.h		\
sftpclient.h sftpcommon.h sftpserver.h status.c thread.h types.h	\
users.c users.h utils.c utils.h v3.c xfns.c xfns.h stat.c charset.c	\
charset.h serialize.h serialize.c v4.c realpath.c readlink.c v5.c v6.c

check: all
	srcdir=${srcdir} ./run-tests
