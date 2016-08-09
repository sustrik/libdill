# unixconnect(3) manual page

## NAME

unixconnect - connect to remote UNIX domain endpoint

## SYNOPSIS

```
#include <dsock.h>
int unixconnect(const char *addr, int64_t deadline);
```

## DESCRIPTION

Connects to a remote UNIX domain endpoint. `addr` is the filename to connect to.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `ECONNREFUSED`: The target address was not listening for connections or refused the connection request.
* `ECONNRESET`: Remote host reset the connection request.
* `EIO`: An I/O error occurred while reading from or writing to the file system.
* `ELOOP`: A loop exists in symbolic links encountered during resolution of the pathname in address.
* `ENAMETOOLONG`: A component of a pathname exceeded NAME_MAX characters, or an entire pathname exceeded PATH_MAX characters.
* `ENOENT`: A component of the pathname does not name an existing file or the pathname is an empty string.
* `ENOTDIR`: A component of the path prefix of the pathname in address is not a directory.
* `EINTR`: The function was interrupted by a signal.
* `EMFILE`: The maximum number of file descriptors in the process are already open.
* `ENFILE`: The maximum number of file descriptors in the system are already open.
* `EACCES`: The process does not have appropriate privileges.
* `ENOBUFS`: No buffer space is available.
* `ENOMEM`: There was insufficient memory available to complete the operation.

## EXAMPLE

```
int s = unixconnect("/tmp/unix.test", -1);
```

## SEE ALSO

* [ipremote(3)](ipremote.html)
* [unixaccept(3)](unixaccept.html)
* [unixlisten(3)](unixlisten.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

