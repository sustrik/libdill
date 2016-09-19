# msend(3) manual page

## NAME

msend - read a message to a socket

## SYNOPSIS

```
#include <dsock.h>
int msend(int s, const void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Writes message to a message socket, such as UDP or WebSocket socket.

The operation is atomic. Either it succeeds to send entire message or fails and sends nothing.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EDADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `EINTR`: The function was interrupted by a signal.
* `ENOTSUP`: Handle is not a message socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
size_t sz = 3;
int rc = msend(s, "ABC", &sz, -1);
```

