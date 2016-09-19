# udpsend(3) manual page

## NAME

udpsend - send a message to UDP socket

## SYNOPSIS

```
#include <dsock.h>
int udpsend(int s, const ipaddr *addr, const void *buf, size_t *len);
```

## DESCRIPTION

Send a UDP packet. If `addr` is not `NULL` the packet will be sent to that address. If it is `NULL` remote address specified in the call to `udpsocket` will be used instead. If even remote address wasn't specified the function will fail with `EINVAL` error.

The operation is atomic. Either it succeeds to send entire packet or fails and sends nothing.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EDADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `EINTR`: The function was interrupted by a signal.
* `ENOTSUP`: Handle is not a UDP socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
size_t sz = 3;
int rc = udpsend(s, NULL, "ABC", &sz, -1);
```

