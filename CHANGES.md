# Release History

## Change in version 3

* In protocol v3, clamp out-of-range timestamps rather than erroring. [Fixes #14](https://github.com/ewxrjk/sftpserver/issues/14).
* Subsecond timestamps are now returned on POSIX platforms, where supported. [Fixes #12](https://github.com/ewxrjk/sftpserver/issues/12).
* In protocol v3, correctly report large file sizes in the human-readable part of the output.  [Fixes #15](https://github.com/ewxrjk/sftpserver/issues/15).
* The SFTP client tracks the working directory properly. [Fixes #11](https://github.com/ewxrjk/sftpserver/issues/11).

## Changes in version 2

* Fix interpretation of malformed handles
* Correct attribute ordering. Bug fix from Bernd Holzmüller.
* Tests use Python 3
* Build fixes

## Changes in version 1

* Fix attribute formatting in v3 directory listings.
* Fix handling of empty paths. [Fixes #3](https://github.com/ewxrjk/sftpserver/issues/3).
* New SFTP extensions, contributed by Michel Kraus:
`fsync@openssh.com`, `hardlink@openssh.com`, `posix-rename@openssh.com`.
On Linux only, `statvfs@openssh.com` and `fstatvfs@openssh.com`.
* Build Fixes

## Changes in version 0.2.2

This is a security release.

* Enforce protocol-specific attribute validity.
* Reject attempts to close malformed handles.

## Changes in version 0.2.1

This is a security release.

* Disable the ‘dumpable’ flag on Linux.

# Older changes

See git hisotry.
