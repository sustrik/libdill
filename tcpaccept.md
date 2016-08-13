# tcpaccept(3) manual page

## NAME

tcpaccept - accept an incoming TCP connection

## SYNOPSIS

```
#include <dsock.h>
int tcpaccept(int s, ipaddr *addr, int64_t deadline);
```

## DESCRIPTION

Accepts an incoming connection from a listening TCP socket. If `addr` is not `NULL` the address of the peer will be filled in.

## RETURN VALUE

Returns handle of the accepted connection. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EBADF`: Invalid handle.
* `ECONNABORTED`: A connection has been aborted.
* `EINTR`: The function was interrupted by a signal.
* `EMFILE`: The maximum number of file descriptors in the process are already open.
* `ENFILE`: The maximum number of file descriptors in the system are already open.
* `ENOBUFS`: No buffer space is available.
* `ENOMEM`: There was insufficient memory available to complete the operation.
* `ENOTSUP`: Handle is not a TCP listening socket.
* `ETIMEDOUT`: Deadline was reached.

## EXAMPLE

```
ipaddr addr;
int rc = iplocal(&addr, NULL, 5555, 0);
int listener = tcplisten(&addr, 10);
int connection = tcpaccept(listener, NULL, -1);
```

