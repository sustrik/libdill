# brecv(3) manual page

## NAME

brecv - read data from a bytestream socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t brecv(int s, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Reads data from a bytestream socket, such as TCP or UNIX domain socket.

## RETURN VALUE

Number of bytes read. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
char buf[256];
ssize_t sz = brecv(s, buf, sizeof(buf), -1);
```

## SEE ALSO

* [bsend(3)](bsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

