# unixlisten(3) manual page

## NAME

unixlisten - start listening for incoming UNIX domain connections

## SYNOPSIS

```
#include <dsock.h>
int unixlisten(const char *addr, int backlog);
```

## DESCRIPTION

Starts listening for incoming UNIX domain connections. `addr` is the file to listen on, `backlog` is the maximum number of received but not yet accepted connections.

## RETURN VALUE

Handle of the created socket. In case of error -1 is returned and `errno` is set to one of the error codes below.

## ERRORS

TODO

## EXAMPLE

```
int s = unixlisten("/tmp/unix.test", 10);
```

## SEE ALSO

* [unixaccept(3)](unixaccept.html)
* [unixconnect(3)](unixconnect.html)
* [unixpair(3)](unixpair.html)
* [unixrecv(3)](unixrecv.html)
* [unixsend(3)](unixsend.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

