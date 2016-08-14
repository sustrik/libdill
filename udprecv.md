# udprecv(3) manual page

## NAME

udprecv - read a message from UDP socket

## SYNOPSIS

```
#include <dsock.h>
int udprecv(int s, ipaddr *addr, void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Reads a pakcet from a UDP socket. When calling the function `len` must be set to the szie of the buffer `buf`. When the function returns it sets `len` to number of bytes in the message. If the message is longer than the buffer it is truncated but `len` still contains the full size of the message.

If `addr` is not `NULL` the source IP address of the packet will be filled into it.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `EINTR`: The function was interruped by a singal.
* `ENOTSUP`: Handle is not a UDP socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
ipaddr src;
char buf[2048];
size_t sz = sizeof(buf);
int rc = udprecv(s, buf, &sz, &src, -1);
```

