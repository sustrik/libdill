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

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
ipaddr addr;
int rc = iplocal(&addr, NULL, 5555, 0);
int listener = tcplisten(&addr, 10);
int connection = tcpaccept(listener, NULL, -1);
```

## SEE ALSO

* [tcpconnect(3)](tcpconnect.html)
* [tcplisten(3)](tcplisten.html)
* [tcprecv(3)](tcprecv.html)
* [tcpsend(3)](tcpsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

