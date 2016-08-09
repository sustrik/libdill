# unixrecv(3) manual page

## NAME

unixrecv - read data from a UNIX domain connection

## SYNOPSIS

```
#include <dsock.h>
ssize_t unixrecv(int s, void *buf, size_t len, int64_t deadline);
```

## DESCRIPTION

Reads data from a UNIX domain socket.

## RETURN VALUE

Number of bytes read. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

* `EDADF`: Invalid handle.
* `ENOTSUP`: Handle is not a UNIX domain socket.

## EXAMPLE

```
char buf[256];
ssize_t sz = unixrecv(s, buf, sizeof(buf), -1);
```

## SEE ALSO

* [brecv(3)](brecv.html)
* [unixaccept(3)](unixaccept.html)
* [unixconnect(3)](unixconnect.html)
* [unixlisten(3)](unixlisten.html)
* [unixpair(3)](unixpair.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

