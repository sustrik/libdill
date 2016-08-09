# unixsend(3) manual page

## NAME

unixsend - send data to a UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t unixsend(int s, const void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Write data to UNIX domain socket.

## RETURN VALUE

Number of bytes written. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ENOTSUP`: Handle is not a UNIX domain socket.

## EXAMPLE

```
ssize_t sz = unixsend(s, "ABC", 3, -1);
```

## SEE ALSO

* [bsend(3)](bsend.html)
* [unixaccept(3)](unixaccept.html)
* [unixconnect(3)](unixconnect.html)
* [unixlisten(3)](unixlisten.html)
* [unixpair(3)](unixpair.html)
* [unixrecv(3)](unixrecv.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

