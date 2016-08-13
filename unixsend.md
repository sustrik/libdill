# unixsend(3) manual page

## NAME

unixsend - send data to a UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
int unixsend(int s, const void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Attempts to write `len` bytes from buffer `buf` to the socket. Function succeeds only if all the bytes are written. If it fails though it may still have written some bytes to the socket. In either case the value of `len` is updated to indicate the number of bytes acutally written.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `ENOTSUP`: Handle is not a UNIX domain socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
size_t sz = 3;
int rc = unixsend(s, "ABC", &sz, -1);
```

