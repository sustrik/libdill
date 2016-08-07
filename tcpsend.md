# tcpsend(3) manual page

## NAME

tcpsend - send data to a TCP connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t tcpsend(int s, const void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Write data to TCP socket.

## RETURN VALUE

Number of bytes written or -1 in case of error.

## ERRORS

TODO

## EXAMPLE

```
ssize_t sz = bsend(s, "ABC", 3, -1);
```

## SEE ALSO

* [bsend(3)](bsend.html)
* [tcpaccept(3)](tcpaccept.html)
* [tcpconnect(3)](tcpconnect.html)
* [tcplisten(3)](tcplisten.html)
* [tcprecv(3)](tcprecv.html)s

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

