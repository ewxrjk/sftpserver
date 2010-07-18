#! /bin/sh

set -e
set -x

# Find an automake
if [ -z "$AUTOMAKE" ]; then
  for prog in automake automake-1.10 automake-1.9 automake-1.8 automake-1.7; do
    if type $prog >/dev/null 2>&1; then
      AUTOMAKE=$prog
      break
    fi
  done
  if [ -s "$AUTOMAKE" ]; then
    echo "ERROR: no automake found" >&2
    exit 1
  fi
fi
ACLOCAL=${AUTOMAKE/automake/aclocal}

srcdir=$(dirname $0)
here=$(pwd)
cd $srcdir
if [ ! -e COPYING ]; then
  rm -f COPYING
  for f in /usr/share/common-licenses/GPL-2 $HOME/doc/GPL-2; do
    if test -e "$f"; then
      ln -s "$f" COPYING
      break
    fi
  done
fi
if test -d $HOME/share/aclocal; then
  ${ACLOCAL} --acdir=$HOME/share/aclocal
else
  ${ACLOCAL}
fi
grep ^AC_PROG_LIBTOOL configure.ac && libtoolize
mkdir -p config.aux
autoconf
autoheader
${AUTOMAKE} -a || true		# for INSTALL
${AUTOMAKE} --foreign -a
cd "$here"
$srcdir/configure -C "$@" --sysconfdir=/etc --localstatedir=/var
