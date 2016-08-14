# unixrecvfd(3) manual page

## NAME

unixrecvfd - read a file descriptor from a UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
int unixrecvfd(int s, int64_t deadline);
```

## DESCRIPTION

Reads one byte from UNIX domain connection and discards it. Returns file descriptor attached to that byte. 

## RETURN VALUE

File descriptor read from the connection. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `EINTR`: The function was interruped by a singal.
* `ENOMSG`: There was no file descriptor attached to the byte.
* `ENOTSUP`: Handle is not a UNIX socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
int fd = unixrecvfd(s, -1);
```

