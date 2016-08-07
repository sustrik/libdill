# bsend(3) manual page

## NAME

bsend - send data to a bytestream socket

## SYNOPSIS

```
#include <dsock.h>
ssize_t bsend(int s, const void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Writes data to a bytestream socket, such as TCP or UNIX domain socket.

## RETURN VALUE

Number of bytes sent. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
ssize_t sz = bsend(s, "ABC", 3, -1);
```

## SEE ALSO

* [brecv(3)](brecv.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

