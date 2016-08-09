# tcpconnect(3) manual page

## NAME

tcpconnect - connect to remote TCP endpoint

## SYNOPSIS

```
#include <dsock.h>
int tcpconnect(const ipaddr *addr, int64_t deadline);
```

## DESCRIPTION

Connects to a remote TCP endpoint. `addr` is the IP address to connect to (see [ipremote(3)](ipremote.html)).

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `ECONNREFUSED`: The target address was not listening for connections or refused the connection request.
* `ECONNRESET`: Remote host reset the connection request.
* `EHOSTUNREACH`: The destination host cannot be reached.
* `ENETDOWN`: The local network interface used to reach the destination is down.
* `ENETUNREACH`: No route to the network is present.
* `EINTR`: The function was interrupted by a signal.
* `EMFILE`: The maximum number of file descriptors in the process are already open.
* `ENFILE`: The maximum number of file descriptors in the system are already open.
* `EACCES`: The process does not have appropriate privileges.
* `ENOBUFS`: No buffer space is available.
* `ENOMEM`: There was insufficient memory available to complete the operation.
* `ETIMEDOUT`: Deadline was reached.

## EXAMPLE

```
ipaddr addr;
int rc = ipremote(&addr, "192.168.0.1", 5555, 0, -1);
int s = tcpconnect(&addr, -1);
```

## SEE ALSO

* [ipremote(3)](ipremote.html)
* [tcpaccept(3)](tcpaccept.html)
* [tcplisten(3)](tcplisten.html)
* [tcprecv(3)](tcprecv.html)
* [tcpsend(3)](tcpsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

