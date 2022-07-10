# This file is part of the Green End SFTP Server.
# Copyright (C) 2007, 2011,14,15,18 Richard Kettlewell
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
AC_DEFUN([RJK_BUILDSYS_FINK],[
  AC_CANONICAL_BUILD
  AC_CANONICAL_HOST
  AC_PATH_PROG([FINK],[fink],[none],[$PATH:/sw/bin])
  if test "x$FINK" != xnone && test "$host" = "$build"; then
    AC_CACHE_CHECK([fink install directory],[rjk_cv_finkprefix],[
      rjk_cv_finkprefix="`echo "$FINK" | sed 's,/bin/fink$,,'`"
    ])
    CPPFLAGS="${CPPFLAGS} -isystem /usr/include -isystem ${rjk_cv_finkprefix}/include"
    LDFLAGS="${LDFLAGS} -L/usr/lib -L${rjk_cv_finkprefix}/lib"
  fi
])

AC_DEFUN([RJK_ISCLANG],[
  AC_CACHE_CHECK([whether compiling with Clang],[rjk_cv_isclang],[
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[
                      #ifndef __clang__
                        not clang
                      #endif]])],
     [rjk_cv_isclang=yes], [rjk_cv_isclang=no])])])])])

AC_DEFUN([RJK_BUILDSYS_MISC],[
  AC_CANONICAL_BUILD
  AC_CANONICAL_HOST
  AC_CACHE_CHECK([for extra include directories],[rjk_cv_extraincludes],[
    rjk_cv_extraincludes=none
    # If we're cross-compiling then we've no idea where to look for
    # extra includes
    if test "$host" = "$build"; then
      case $host_os in
      freebsd* )
        rjk_cv_extraincludes=/usr/local/include
        ;;
      esac
    fi
  ])
  if test $rjk_cv_extraincludes != none; then
    if test $GCC = yes; then
      CPPFLAGS="-isystem $rjk_cv_extraincludes"
    else
      CPPFLAGS="-I$rjk_cv_extraincludes"
    fi
  fi
  AC_CACHE_CHECK([for extra library directories],[rjk_cv_extralibs],[
    rjk_cv_extralibs=none
    if test "$host" = "$build"; then
      case $host_os in
      freebsd* )
        rjk_cv_extralibs=/usr/local/lib
        ;;
      esac
    fi
  ])
  if test $rjk_cv_extralibs != none; then
    LDFLAGS="-L$rjk_cv_extralibs"
  fi
])

AC_DEFUN([RJK_GCC_WARNINGS],[
  RJK_ISCLANG
  AC_CACHE_CHECK([for ${CC} warning options],[rjk_cv_ccwarnings],[
    if test $rjk_cv_isclang = no; then
      suppress=-Wno-type-limits
    else
      suppress=-Wno-tautological-constant-out-of-range-compare
    fi
    if test "$GCC" = yes; then
      rjk_cv_ccwarnings="-Wall -W -Wpointer-arith -Wbad-function-cast \
$suppress \
-Wwrite-strings -Wmissing-prototypes \
-Wmissing-declarations -Wnested-externs"
    else
      rjk_cv_ccwarnings="unknown"
    fi
  ])
  AC_CACHE_CHECK([how to make ${CC} treat warnings as errors],
                 [rjk_cv_ccwerror],[
    if test "$GCC" = yes; then
      rjk_cv_ccwerror="-Werror"
    else
      rjk_cv_ccwerror="unknown"
    fi
  ])
  AC_MSG_CHECKING([whether to enable compiler warnings])
  AC_ARG_ENABLE([warnings],
		[AS_HELP_STRING([--disable-warnings],
				[Disable compiler warnings])],
		[warnings="$enableval"],
		[warnings=yes])
  AC_MSG_RESULT([$warnings])
  AC_MSG_CHECKING([whether to treat warnings as errors])
  AC_ARG_ENABLE([warnings-as-errors],
                [AS_HELP_STRING([--disable-warnings-as-errors],
                                [Don't treat compiler warnings as errors])],
		[warnings_as_errors="$enableval"],
		[warnings_as_errors=yes])
  AC_MSG_RESULT([$warnings_as_errors])
  if test "$warnings" = yes && test "$rjk_cv_ccwarnings" != unknown; then
    CC="${CC} $rjk_cv_ccwarnings"
  fi
  if test "$warnings_as_errors" = yes && test "$rjk_cv_ccwerror" != unknown; then
    CC="${CC} $rjk_cv_ccwerror"
  fi
  AC_CACHE_CHECK([whether <inttypes.h> macros produce warnings],
                 [rjk_cv_inttypeswarnings],[
    AC_TRY_COMPILE([#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>],
                   [uint64_t x=0;size_t sz=0;printf("%"PRIu64" %zu\n", x, sz);],
                   [rjk_cv_inttypeswarnings=no],
                   [rjk_cv_inttypeswarnings=yes])
  ])
  if test $rjk_cv_inttypeswarnings = yes && test "$GCC" = yes; then
    CC="${CC} -Wno-format"
  fi
])

AC_DEFUN([RJK_GTKFLAGS],[
  AM_PATH_GLIB_2_0([],[],[missing_libraries="$missing_libraries libglib"])
  AM_PATH_GTK_2_0([],[],[missing_libraries="$missing_libraries libgtk"])
  if test "$GCC" = yes; then
    GTK_CFLAGS="`echo \"$GTK_CFLAGS\"|sed 's/-I/-isystem /g'`"
    GLIB_CFLAGS="`echo \"$GLIB_CFLAGS\"|sed 's/-I/-isystem /g'`"
  fi
])

AC_DEFUN([RJK_STAT_TIMESPEC],[
  AC_CACHE_CHECK([for timespec style in struct stat],[rjk_cv_stat_timespec],[
    rjk_cv_stat_timespec=none
    AC_TRY_COMPILE([#include <sys/stat.h>],[
      struct stat sb;
      sb.st_atim.tv_sec = 0;
      (void)sb;
    ],[rjk_cv_stat_timespec=POSIX])
    AC_TRY_COMPILE([#include <sys/stat.h>],[
      struct stat sb;
      sb.st_atimespec.tv_sec = 0;
      (void)sb;
    ],[rjk_cv_stat_timespec=BSD])
  ])
  case "$rjk_cv_stat_timespec" in
  BSD )
    AC_DEFINE([ST_ATIM],[st_atimespec],[define to last access time field])
		AC_DEFINE([ST_MTIM],[st_mtimespec],[define to last modification time field])
		AC_DEFINE([ST_CTIM],[st_Ctimespec],[define to creation time field])
    ;;
  POSIX )
    AC_DEFINE([ST_ATIM],[st_atim],[define to last access time field])
    AC_DEFINE([ST_MTIM],[st_mtim],[define to last modification time field])
    AC_DEFINE([ST_CTIM],[st_ctim],[define to creation time field])
    ;;
  esac
])

AC_DEFUN([RJK_STAT_TIMESPEC_POSIX],[
  AC_CHECK_MEMBER([struct stat.st_atim],
		  [AC_DEFINE([HAVE_STAT_TIMESPEC_POSIX],[1],
		             [define if struct stat uses struct timespec (POSIX variant)])],
		  [rjk_cv_stat_timespec=no],
		  [#include <sys/stat.h>])
  ])
])

AC_DEFUN([RJK_GCC_ATTRS],[
  AH_BOTTOM([#ifdef __GNUC__
# define attribute(x) __attribute__(x)
#else
# define attribute(x)
#endif])
  ])
])

AC_DEFUN([RJK_ICONV],[
  # MacOS has a rather odd iconv (presumably for some good reason)
  AC_CHECK_LIB([iconv],[iconv_open])
  AC_CHECK_LIB([iconv],[libiconv_open])
])

AC_DEFUN([RJK_GCOV],[
  GCOV=${GCOV:-true}
  AC_ARG_WITH([gcov],
              [AS_HELP_STRING([--with-gcov],
                              [Enable coverage testing])],
              [if test $withval = yes; then
                 CFLAGS="${CFLAGS} -O0 -fprofile-arcs -ftest-coverage"
                 GCOV=`echo $CC | sed s'/gcc/gcov/;s/ .*$//'`;
               fi])
  AC_SUBST([GCOV],[$GCOV])
])

AC_DEFUN([RJK_THREADS],[
  AC_CANONICAL_BUILD
  AC_CANONICAL_HOST
  # If you're cross-compiling then you're on your own
  AC_MSG_CHECKING([how to build threaded code])
  if test "$host" = "$build"; then
    case $host_os in
    solaris2* )
      case "$GCC" in
      yes )
        AC_MSG_RESULT([-lpthread])
        AC_CHECK_LIB([pthread],[pthread_create])
        ;;
      * )
        AC_MSG_RESULT([-mt option])
        CC="${CC} -mt"
        ;;
      esac
      ;;
    * )
      # Guess that unrecognized things are like Linux/BSD
      AC_MSG_RESULT([-lpthread])
      AC_CHECK_LIB([pthread],[pthread_create])
      ;;
    esac
  fi
  # We always ask for this.
  AC_DEFINE([_REENTRANT],[1],[define for re-entrant functions])
])

AC_DEFUN([RJK_SIZE_MAX],[
  AC_CHECK_SIZEOF([unsigned short])
  AC_CHECK_SIZEOF([unsigned int])
  AC_CHECK_SIZEOF([unsigned long])
  AC_CHECK_SIZEOF([unsigned long long])
  AC_CHECK_SIZEOF([size_t])
  AC_CHECK_HEADERS([stdint.h])
  AC_CACHE_CHECK([for SIZE_MAX],[rjk_cv_size_max],[
    AC_TRY_COMPILE([#include <limits.h>
                    #include <stddef.h>
                    #if HAVE_STDINT_H
                    # include <stdint.h>
                    #endif],
                   [size_t x = SIZE_MAX;++x;],
                   [rjk_cv_size_max=yes],
                   [rjk_cv_size_max=no])
  ])
  if test "$rjk_cv_size_max" = yes; then
    AC_DEFINE([HAVE_SIZE_MAX],[1], [define if you have SIZE_MAX])
  fi
  AH_BOTTOM([#if ! HAVE_SIZE_MAX
# if SIZEOF_SIZE_T == SIZEOF_UNSIGNED_SHORT
#  define SIZE_MAX USHRT_MAX
# elif SIZEOF_SIZE_T == SIZEOF_UNSIGNED_INT
#  define SIZE_MAX UINT_MAX
# elif SIZEOF_SIZE_T == SIZEOF_UNSIGNED_LONG
#  define SIZE_MAX ULONG_MAX
# elif SIZEOF_SIZE_T == SIZEOF_UNSIGNED_LONG_LONG
#  define SIZE_MAX ULLONG_MAX
# else
#  error Cannot deduce SIZE_MAX
# endif
#endif
  ])
])

AC_DEFUN([RJK_GETOPT],[
  AC_CHECK_FUNC([getopt_long],[],[
    AC_LIBOBJ([getopt])
    AC_LIBOBJ([getopt1])
  ])
])

AC_DEFUN([RJK_PYTHON3],[
  AC_CACHE_CHECK([for Python 3],[rjk_cv_python3],[
    if python3 -V >/dev/null 2>&1; then
      rjk_cv_python3=python3
    elif python -V >confpyver 2>&1; then
      read p v < confpyver
      case "$v" in
      1* | 2* )
        ;;
      * )
        rjk_cv_python3=python
        ;;
      esac
    fi
    rm -f confpyver
    if test "$rjk_cv_python3" = ""; then
      AC_MSG_ERROR([cannot find Python 3.x])
    fi
  ])
  AC_SUBST([PYTHON3],[$rjk_cv_python3])
])

dnl Local Variables:
dnl mode:autoconf
dnl indent-tabs-mode:nil
dnl End:
