# mrecv(3) manual page

## NAME

mrecv - read a message from a socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t mrecv(int s, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Reads a message from a message socket, such as UDP or WebSocket socket.

## RETURN VALUE

Number of bytes read. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ENOTSUP`: Handle is not a message socket.

## EXAMPLE

```
char buf[256];
ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
```

## SEE ALSO

* [msend(3)](msend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

