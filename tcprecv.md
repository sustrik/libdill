# tcprecv(3) manual page

## NAME

tcprecv - read data from a TCP connection

## SYNOPSIS

```
#include <dsock.h>
int tcprecv(int s, void *buf, size_t *len, int64_t deadline);
```

## DESCRIPTION

Reads `len` bytes from the socket to buffer `buf`. When calling the function `len` must be set to the number of bytes requested. When the function returns it sets `len` to number of bytes actually received.

The function succeeds only if it reads all the requested bytes. If it fails it may still have read some bytes. In both cases the number of bytes read is available via `len` argument.

## RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `ECONNRESET`: The connection is broken.
* `EINTR`: The function was interruped by a singal.
* `ENOTSUP`: Handle is not a TCP socket.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

```
char buf[16];
size_t sz = sizeof(buf);
int rc = tcprecv(s, buf, &sz, -1);
```

