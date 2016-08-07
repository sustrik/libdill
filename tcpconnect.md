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

TODO

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

