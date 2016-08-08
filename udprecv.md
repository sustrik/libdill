# udprecv(3) manual page

## NAME

udprecv - read a message from UDP socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t udprecv(int s, ipaddr *addr, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Read a packet from UDP socket. If `addr` is not `NULL` the source IP address of the packet will be filled in.

## RETURN VALUE

Number of bytes read. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
ipaddr src;
char buf[2000];
ssize_t sz = udprecv(s, &src, buf, sizeof(buf), -1);
```

## SEE ALSO

```
* [mrecv(3)](mrecv.html)
* [udpsend(3)](udpsend.html)
* [udpsocket(3)](udpsocket.html)
```

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

