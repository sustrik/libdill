# unixlisten(3) manual page

## NAME

unixlisten - start listening for incoming UNIX domain connections

## SYNOPSIS

```
#include <dsock.h>
int unixlisten(const char *addr, int backlog);
```

## DESCRIPTION

Starts listening for incoming UNIX domain connections. `addr` is the file to listen on, `backlog` is the maximum number of received but not yet accepted connections.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EDESTADDRREQ` or `EISDIR`: The address argument is a null pointer.
* `EIO`: An I/O error occurred.
* `ELOOP`: A loop exists in symbolic links encountered during resolution of the pathname in address.
* `ENAMETOOLONG`: A component of a pathname exceeded NAME_MAX characters, or an entire pathname exceeded PATH_MAX characters.
* `ENOENT`: A component of the pathname does not name an existing file or the pathname is an empty string.
* `ENOTDIR`: A component of the path prefix of the pathname in address is not a directory.
* `EROFS`: The name would reside on a read-only file system.
* `EADDRINUSE`: The specified address is already in use.
* `EMFILE`: No more file descriptors are available for this process.
* `ENFILE`: No more file descriptors are available for the system.
* `EACCES`: The process does not have appropriate privileges.
* `ENOBUFS`: Insufficient resources were available in the system to perform the operation.
* `ENOMEM`: Insufficient memory was available to fulfill the request.

## EXAMPLE

```
int s = unixlisten("/tmp/unix.test", 10);
```

## SEE ALSO

* [unixaccept(3)](unixaccept.html)
* [unixconnect(3)](unixconnect.html)
* [unixpair(3)](unixpair.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

