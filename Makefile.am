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
stat.h getcwd.c globals.c dirname.c putword.h
libsftp_a_LIBADD=$(LIBOBJS)

# Documentation
man_MANS=gesftpserver.8

gesftpserver.8: gesftpserver.8.in config.status
	sed < ${srcdir}/gesftpserver.8.in > $@.new \
		-e "s,__libexecdir__,${libexecdir},g"
	mv $@.new $@

# Build and test
all: ${SEDOUTPUTS}

check: all aliases
	rm -f *.gcda *.gcov
	srcdir=${srcdir} ${PYTHON24} ${srcdir}/run-tests --directory tests $(TESTS)
	srcdir=${srcdir} ${PYTHON24} ${srcdir}/run-tests --directory rotests --server ./gesftpserver-ro $(ROTESTS)
	${GCOV} ${srcdir}/*.c  | ${PYTHON24} ${srcdir}/format-gconv-report --html .

#${srcdir}/paramiko-test

check-valgrind: all aliases
	rm -f ,valgrind*
	srcdir=${srcdir} ${PYTHON24} ${srcdir}/run-tests --directory tests --server ./gesftpserver-valgrind $(TESTS)

aliases: gesftpserver-ro gesftpserver-debug gesftpserver-ro-debug \
		gesftpserver-valgrind

gesftpserver-ro:
	rm -f $@
	ln -s gesftpserver $@

gesftpserver-debug:
	rm -f $@
	ln -s gesftpserver $@

gesftpserver-ro-debug:
	rm -f $@
	ln -s gesftpserver $@

gesftpserver-valgrind:
	echo "#! /bin/sh" > $@.new
	echo "set -e" >> $@.new
	echo "exec valgrind --leak-check=full --logfile=`pwd`/,valgrind --num-callers=64 `pwd`/gesftpserver \"\$$@\"" >> $@.new
	chmod +x $@.new
	mv $@.new $@

# Cleaning
DISTCLEANFILES=${SEDOUTPUTS}
CLEANFILES=*.gcno *.gcda *.gcov gesftpserver-ro gesftpserver-debug gesftpserver-ro-debug gesftpserver-valgrind

# Distribution
EXTRA_DIST=gesftpserver.8.in testing.txt tests rotests README run-tests	\
format-gconv-report getopt.c getopt1.c getopt.h

%.s: %.c
	$(COMPILE) -o $@ -S $<
