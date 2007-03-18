# This file is part of the Green End SFTP Server.
# Copyright (C) 2007 Richard Kettlewell
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

# Programs
noinst_PROGRAMS=sftpclient
noinst_SCRIPTS=run-tests
libexec_PROGRAMS=gesftpserver
noinst_LIBRARIES=libsftp.a

gesftpserver_SOURCES=sftpserver.c readwrite.c
gesftpserver_LDADD=libsftp.a

sftpclient_SOURCES=sftpclient.c readwrite.c
sftpclient_LDADD=libsftp.a $(LIBREADLINE)

libsftp_a_SOURCES=alloc.c alloc.h debug.c debug.h globals.h handle.c	\
handle.h parse.c parse.h queue.c queue.h send.c send.h sftp.h		\
sftpclient.h sftpcommon.h sftpserver.h status.c thread.h types.h	\
users.c users.h utils.c utils.h v3.c xfns.c xfns.h stat.c charset.c	\
charset.h serialize.h serialize.c v4.c realpath.c readlink.c v5.c v6.c	\
stat.h getcwd.c globals.c dirname.c
libsftp_a_LIBADD=$(LIBOBJS)

# Documentation
man_MANS=gesftpserver.8

gesftpserver.8: gesftpserver.8.in config.status
	sed < ${srcdir}/gesftpserver.8.in > $@.new \
		-e "s,__libexecdir__,${libexecdir},g"
	mv $@.new $@

# Build and test
all: ${SEDOUTPUTS}

check: all links
	rm -f *.gcda *.gcov
	srcdir=${srcdir} ${srcdir}/run-tests --directory tests
	srcdir=${srcdir} ${srcdir}/run-tests --directory rotests --server ./gesftpserver-ro
	${GCOV} ${srcdir}/*.c  | ${srcdir}/format-gconv-report --html .

links:
	rm -f gesftpserver-ro gesftpserver-debug gesftpserver-ro-debug
	ln -s gesftpserver gesftpserver-ro
	ln -s gesftpserver gesftpserver-debug
	ln -s gesftpserver gesftpserver-ro-debug

# Cleaning
DISTCLEANFILES=${SEDOUTPUTS}
CLEANFILES=*.gcno *.gcda *.gcov gesftpserver-ro gesftpserver-debug gesftpserver-ro-debug

# Distribution
EXTRA_DIST=gesftpserver.8.in testing.txt tests README run-tests	\
format-gconv-report

%.s: %.c
	$(COMPILE) -o $@ -S $<
