# tcprecv(3) manual page

## NAME

tcprecv - read data from a TCP connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t tcprecv(int s, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Reads data from a TCP socket.

## RETURN VALUE

Number of bytes read. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EBADF`: Invalid handle.
* `ENOTSUP`: Handle is not a TCP socket.

## EXAMPLE

```
char buf[256];
ssize_t sz = tcprecv(s, buf, sizeof(buf), -1);
```

