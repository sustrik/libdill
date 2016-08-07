# tcplisten(3) manual page

## NAME

tcplisten - start listening for incoming TCP connections

## SYNOPSIS

```
#include <dsock.h>
int tcplisten(ipaddr *addr, int backlog);
```

## DESCRIPTION

Starts listening for incoming TCP connections. `addr` is an IP address to
listen on (see [`iplocal(3)`](iplocal.html)), `backlog` is the maximum number
of received but not yet accepted connections.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
ipaddr addr;
int rc = iplocal(&addr, NULL, 5555, 0);
int s = tcplisten(&addr, 10);
```

## SEE ALSO

* [iplocal(3)](iplocal.html)
* [tcpaccept(3)](tcpaccept.html)
* [tcpconnect(3)](tcpconnect.html)
* [tcprecv(3)](tcprecv.html)
* [tcpsend(3)](tcpsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

