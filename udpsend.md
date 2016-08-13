# udpsend(3) manual page

## NAME

udpsend - send a message to UDP socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t udpsend(int s, const ipaddr *addr, const void *buf, size_t len);
```

## DESCRIPTION

Send a UDP packet. If `addr` is not `NULL` the packet will be sent to that address. If it is `NULL` remote address specified in the call to `udpsocket` will be used instead. If even remote address wasn't specified the function will fail with `EINVAL` error.

## RETURN VALUE

Number of bytes sent. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ENOTSUP`: Handle is not a UDP socket.

## EXAMPLE

```
ssize_t sz = udpsend(s, NULL, "ABC", 3, -1);
```

