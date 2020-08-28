# Release History

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
