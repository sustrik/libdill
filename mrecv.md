# mrecv(3) manual page

## NAME

mrecv - read a message from a socket

## SYNOPSIS

```
#include <dsock.h>
int mrecv(int s, void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Reads a message from a message socket, such as UDP or WebSocket socket. When calling the function `len` must be set to the szie of the buffer `buf`. When the function returns it sets `len` to number of bytes in the message. If the message is longer than the buffer it is truncated but `len` still contains the full size of the message.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `ENOTSUP`: Handle is not a message socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
char buf[2048];
size_t sz = sizeof(buf);
int rc = mrecv(s, buf, &sz, -1);
```

