# unixsendfd(3) manual page

## NAME

unixsendfd - send a file descriptor through a UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t unixsendfd(int s, int fd, int64_t deadline);
```

## DESCRIPTION

Sends one byte (0) to the UNIX domain connection and attaches file descriptor `fd` to it.

The file descriptor is duplicated during the transport. The caller is still responsible for closing it after this call.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EBADF`: Invalid handle.
* `ENOTSUP`: Handle is not a UNIX domain socket.

## EXAMPLE

```
int rc = unixsendfd(s, fd, -1);
```

