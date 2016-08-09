# unixaccept(3) manual page

## NAME

unixaccept - accept an incoming UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
int unixaccept(int s, int64_t deadline);
```

## DESCRIPTION

Accepts an incoming connection from a listening UNIX domain socket.

## RETURN VALUE

Returns handle of the accepted connection. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* *ECONNABORTED*: A connection has been aborted.
* *EINTR*: The function was interrupted by a signal.
* *EMFILE*: The maximum number of file descriptors in the process are already open.
* *ENFILE*: The maximum number of file descriptors in the system are already open.
* *ENOBUFS*: No buffer space is available.
* *ENOMEM*: There was insufficient memory available to complete the operation.
* *ETIMEDOUT*: Deadline was reached.

## EXAMPLE

```
int listener = unixlisten("/tmp/unix.test", 10);
int connection = unixaccept(listener, -1);
```

## SEE ALSO

* [unixconnect(3)](unixconnect.html)
* [unixlisten(3)](unixlisten.html)
* [unixpair(3)](unixpair.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

