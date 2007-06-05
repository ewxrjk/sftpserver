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

# Process this file with autoconf to produce a configure script.
AC_INIT(sftpserver, 0.0+, richard+sftpserver@sfere.greenend.org.uk)
AC_CONFIG_AUX_DIR([config.aux])
AM_INIT_AUTOMAKE([sftpserver], [0.0+])
AC_CONFIG_SRCDIR([alloc.c])
AM_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_SET_MAKE

#AC_PROG_LIBTOOL
#AC_LIBTOOL_DLOPEN
#AC_DISABLE_STATIC
#RJK_BUILDSYS_FINK
RJK_BUILDSYS_MISC
#RJK_GTKFLAGS
AC_PROG_RANLIB

RJK_THREADS
AC_CHECK_LIB([socket],[socket])
AC_CHECK_LIB([readline],[readline],
             [AC_SUBST([LIBREADLINE],[-lreadline])
              AC_DEFINE([HAVE_READLINE],[1],[define if you have a readline library])])
RJK_ICONV
AC_DEFINE([_GNU_SOURCE], [1], [required for e.g. strsignal])
AC_C_INLINE
AC_SYS_LARGEFILE
AC_REPLACE_FUNCS([daemon futimes])
AC_CHECK_FUNCS([futimesat getaddrinfo])

RJK_UNALIGNED_WRITES
RJK_SIZE_MAX
RJK_GCC_WARNINGS
RJK_GCC_ATTRS
RJK_STAT_TIMESPEC
RJK_GETOPT
RJK_PYTHON24

dnl See commentary in v3.c for what's going on here
AC_MSG_CHECKING([whether to reverse SSH_FXP_SYMLINK arguments])
AC_ARG_ENABLE([reversed-symlink],
              [AS_HELP_STRING([--enable-reversed-symlink],
                              [Reverse SSH_FXP_SYMLINK arguments])],
              [revlink="$enableval"],
              [revlink=no])
AC_MSG_RESULT([$revlink])
if test $revlink = yes; then
  AC_DEFINE([REVERSE_SYMLINK],[1],[define to reverse SSH_FXP_SYMLINK args])
fi

AC_ARG_ENABLE([daemon],
              [AS_HELP_STRING([--enable-daemon],
                              [Turn on daemon mode support])],
              [daemon="$enableval"],
              [daemon=no])
if test $daemon = yes; then
  AC_DEFINE([DAEMON],[1],[define to enable daemon mode])
fi

RJK_GCOV

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl Local Variables:
dnl indent-tabs-mode:nil
dnl End:
