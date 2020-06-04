Green End SFTP Server
=====================

[![Build Status](https://travis-ci.org/ewxrjk/sftpserver.svg?branch=master)](https://travis-ci.org/ewxrjk/sftpserver)

This is an SFTP server supporting up to protocol version 6.  It is
possible to use it as a drop-in replacement for the OpenSSH server.

Requirements
------------

This software runs under Linux and macOS.
It may work on other UNIX platforms.

Installation
------------

The general procedure is:

    ./autogen.sh
    ./configure
    make check       # builds software and runs tests
    make install     # probably as root

See [INSTALL](INSTALL) for generic instructions.  Local configure
options include:

* `--enable-reversed-symlink` - some (but annoyingly not all) SFTP
clients get the arguments to the symlink operation the wrong way
round.  This option reverses the server’s interpretation of the
arguments so that such clients can be made to work (while breaking
correctly written clients).
* `--enable-warnings-as-errors` - enable treatment of
warnings-as-errors.  Developers should use this but end users probably
don’t care.

You will need iconv and readline libraries.

It’s best to run `make check` before installing.  This requires
[Python 3](http://www.python.org/) to be installed:

    apt install python3 python3-termcolors

Status
------

The code is written to be secure against malicious clients but not
routinely tested against malicious clients (although it has undergone fuzzing).
In the typical usage
pattern for an SFTP server this is academic: why bother exploiting a
SFTP server bug when you can log in as the same user?

However it is also possible to arrange for the SFTP server to run on
a security boundary, for instance listening directly on a TCP port
or running under a login that is only allowed to run the SFTP
server.

Until such time as the test suite is expanded to include actively
hostile clients, it is not recommended that this server be used in
such configurations without some additional form of protection
against malicious use.

Online Resources
----------------

* [sgo-software-discuss](http://www.chiark.greenend.org.uk/mailman/listinfo/sgo-software-discuss): discussion of the server (and others), bug reports, etc
* [sgo-software-announce](http://www.chiark.greenend.org.uk/mailman/listinfo/sgo-software-announce): announcements of new versions
* [www.greenend.org.uk/rjk/sftpserver](http://www.greenend.org.uk/rjk/sftpserver): home page
* [github.com/ewxrjk/sftpserver](https://github.com/ewxrjk/sftpserver): source code

To get the latest source (to within 24 hours):

    git clone git://github.com/ewxrjk/sftpserver.git

New releases are signed with PGP key
[9006B0ED710DB81B76D368D9D3388BA18A741BEF](http://www.greenend.org.uk/rjk/misc/8A741BEF.asc).

Bugs, Patches, etc
------------------

Please either send bug reports, improvements, etc to
[sgo-software-discuss](http://www.chiark.greenend.org.uk/mailman/listinfo/sgo-software-discuss)
or use [GitHub](https://github.com/ewxrjk/sftpserver).

If you send patches, please use `diff -u` format.

Licence
-------

Copyright (C) 2007, 2009-2011, 2014-18 Richard Kettlewell

Portions copyright (C) 2015 Michel Kraus

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA
