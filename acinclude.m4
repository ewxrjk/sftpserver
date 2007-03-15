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
  AC_CACHE_CHECK([for ${CC} warning options],[rjk_cv_ccwarnings],[
    if test "$GCC" = yes; then
      rjk_cv_ccwarnings="-Wall -W -Wpointer-arith -Wbad-function-cast \
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
                [AS_HELP_STRING([--enable-warnings-as-errors],
                                [Treat compiler warnings as errors])],
		[warnings_as_errors="$enableval"],
		[warnings_as_errors=no])
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

AC_DEFUN([RJK_UNALIGNED_WRITES],[
  AC_CACHE_CHECK([whether unaligned writes work],[rjk_cv_unaligned],[
    AC_TRY_RUN([int main(void) {
  union { long n; char c@<:@1 + sizeof (long)@:>@; } u;
  *(long *)(u.c + 1) = 0;
  return 0;
}],
               [rjk_cv_unaligned=yes],
               [rjk_cv_unaligned=no],
               [rjk_cv_unaligned=unknown])
  ])
  if test $rjk_cv_unaligned = yes; then
    AC_DEFINE([UNALIGNED_WRITES],[1],[define if unaligned writes  work])
  fi
])

AC_DEFUN([RJK_STAT_TIMESPEC],[
  AC_CHECK_MEMBER([struct stat.st_atimespec],
		  [AC_DEFINE([HAVE_STAT_TIMESPEC],[1],
		             [define if struct stat uses struct timespec])],
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
        AC_MSG_RESULT([-D_REENTRANT -lpthread])
        AC_DEFINE([_REENTRANT],[1],[define to enable thread-safe API])
        AC_CHECK_LIB([pthread],[pthread_create])
        ;;
      * )
        AC_MSG_RESULT([-mt option])
        CC="${CC} -mt"
        ;;
      esac
      ;;
    linux* )
      AC_MSG_RESULT([-lpthread])
      AC_CHECK_LIB([pthread],[pthread_create])
      ;;
    freebsd* ) 
      AC_MSG_RESULT([-lpthread])
      AC_CHECK_LIB([pthread],[pthread_create])
      ;;
    * )
      AC_MSG_RESULT([unknown])
      AC_MSG_ERROR([don't know how to build threaded code on this system])
      ;;
    esac
  fi
])

dnl Local Variables:
dnl mode:autoconf
dnl indent-tabs-mode:nil
dnl End:
