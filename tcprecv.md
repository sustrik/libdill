# tcprecv(3) manual page

## NAME

tcprecv - read data from a TCP connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t tcprecv(int s, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Reads data from a TCP socket.

## RETURN VALUE

Number of bytes read or -1 in case of error.

## ERRORS

TODO

## EXAMPLE

```
char buf[256];
ssize_t sz = brecv(s, buf, sizeof(buf), -1);
```

## SEE ALSO

* [brecv(3)](brecv.html)
* [tcpaccept(3)](tcpaccept.html)
* [tcpconnect(3)](tcpconnect.html)
* [tcplisten(3)](tcplisten.html)
* [tcpsend(3)](tcpsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

