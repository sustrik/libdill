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

Number of bytes written. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EBADF`: Invalid handle.
* `ENOTSUP`: Handle is not a TCP socket.

## EXAMPLE

```
ssize_t sz = bsend(s, "ABC", 3, -1);
```

