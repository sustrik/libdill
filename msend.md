# msend(3) manual page

## NAME

msend - read a message to a socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t msend(int s, const void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Writes message to a message socket, such as UDP or WebSocket socket.

## RETURN VALUE

Number of bytes written. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ENOTSUP`: Handle is not a message socket.

## EXAMPLE

```
ssize_t sz = msend(s, "ABC", 3, -1);
```

