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
RJK_BUILDSYS_FINK
#RJK_GTKFLAGS

AC_CHECK_HEADERS([pthread.h])
AC_DEFINE(_GNU_SOURCE, 1, [required for e.g. strsignal])
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_INLINE

RJK_UNALIGNED_WRITES
RJK_GCC_WARNINGS
RJK_GCC_ATTRS

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
