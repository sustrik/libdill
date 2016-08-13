# bsend(3) manual page

## NAME

bsend - send data to a bytestream socket

## SYNOPSIS

```
#include <dsock.h>
int bsend(int s, const void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Writes data to a bytestream socket, such as TCP or UNIX domain socket. Function succeeds only if all the bytes are written. If it fails though it may still have written some bytes to the socket. In either case the value of `len` is updated to indicate the number of bytes acutally written.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `ENOTSUP`: Handle is not a bytestream socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
size_t sz = 3;
int rc = bsend(s, "ABC", 3, -1);
```

